#!/usr/bin/env node
// Desktop i18n source-vs-catalog gate (no Qt needed). Complements, does not replace:
//   - scripts/check-i18n-parity.mjs   (repo root; key parity ACROSS locales, derived only from the .ts files), and
//   - the CI `lupdate -no-obsolete` diff in desktop-ci (exact contexts/line data; needs Qt).
// The parity script cannot see a string that is in no catalog at all (empty catalogs pass); this one reads the
// strings from the QML/C++ sources and requires each of them in every locale.
//
//   node apps/desktop/scripts/check-i18n-extract.mjs [--root <apps/desktop-like dir>] [--json]
//
// Rules (ids are printed in brackets; scripts/selftest/ has a fixture that must trip each):
//   i18n-missing      a qsTr()/tr()/qsTranslate()/QCoreApplication::translate() source string is absent from a locale
//   i18n-unfinished   a catalog message is unfinished/vanished/obsolete or has an empty translation
//   i18n-stale        a catalog message whose source string no longer occurs in the module's sources
//   i18n-numerus      a "%n" source is not numerus="yes" with enough non-empty <numerusform>s in a locale
//   i18n-placeholder  %1..%9 / %n placeholders differ between source and translation
//   i18n-nonliteral   tr()/qsTr() called with a non-literal first argument (lupdate cannot extract it)
// Matching is by source text (+ optional disambiguation is ignored); the context is not compared here because C++
// contexts need a C++ parser. lupdate in CI is the exact check. Locales: en, ru, tg, uz, ar.
// Escape hatch for a deliberate non-literal call: `// aedst-allow: <reason>` on the line or the line above.
import { readdirSync, readFileSync, writeFileSync } from "node:fs";
import { basename, dirname, join, relative, sep } from "node:path";
import { fileURLToPath } from "node:url";

const LOCALES = ["en", "ru", "tg", "uz", "ar"];
// Minimum number of <numerusform> per locale (UNVERIFIED against Qt's numerus table; lupdate in CI fixes the exact count).
const MIN_NUMERUS_FORMS = { en: 2, ru: 3, tg: 2, uz: 2, ar: 6 };

const args = process.argv.slice(2);
const argValue = (flag) => {
  const i = args.indexOf(flag);
  return i >= 0 ? args[i + 1] : undefined;
};
const root = argValue("--root") ?? join(dirname(fileURLToPath(import.meta.url)), "..");
const asJson = args.includes("--json");
// --lupdate-root <dir>: take the expected strings from catalogs that `lupdate -no-obsolete` regenerated under <dir>
// (same layout as --root) instead of the regex extraction. Used by scripts/check-i18n-lupdate.sh in CI (needs Qt).
const lupdateRoot = argValue("--lupdate-root");
// Known gaps (shrink-only): per catalog, how many strings are missing/unfinished today. Only the real tree has one.
const defaultBaseline = join(dirname(fileURLToPath(import.meta.url)), "i18n-baseline.json");
const baselineFile = argValue("--baseline") ?? (argValue("--root") ? undefined : defaultBaseline);
const updateBaseline = args.includes("--update-baseline");

const SKIP_DIRS = new Set(["build", "node_modules", "tests", "i18n", "third_party", "generated", "selftest"]);
const walk = (dir, out = []) => {
  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    if (SKIP_DIRS.has(entry.name) || entry.name.startsWith(".")) continue;
    const full = join(dir, entry.name);
    if (entry.isDirectory()) walk(full, out);
    else out.push(full);
  }
  return out;
};

/** Directories that own an i18n/<module>_<locale>.ts set. */
const findModules = () => {
  const modules = [];
  const visit = (dir) => {
    for (const entry of readdirSync(dir, { withFileTypes: true })) {
      if (["build", "node_modules", "third_party", "selftest", ".git"].includes(entry.name)) continue;
      const full = join(dir, entry.name);
      if (!entry.isDirectory()) continue;
      if (entry.name === "i18n") {
        const catalogs = readdirSync(full).filter((f) => /^[a-z0-9]+_[a-z]{2}\.ts$/.test(f));
        if (catalogs.length > 0) modules.push({ dir, name: catalogs[0].replace(/_[a-z]{2}\.ts$/, ""), i18n: full });
      } else visit(full);
    }
  };
  visit(root);
  return modules;
};

const unescapeJs = (body) =>
  body.replace(/\\(u[0-9a-fA-F]{4}|x[0-9a-fA-F]{2}|.)/g, (_m, c) => {
    if (c[0] === "u" || c[0] === "x") return String.fromCharCode(parseInt(c.slice(1), 16));
    return { n: "\n", t: "\t", r: "\r", "0": "\0" }[c] ?? c;
  });

/** Reads string literals starting at `i` (adjacent in C++, joined by `+` in QML). Returns { text, end } or null. */
const readLiteral = (src, i) => {
  let text = "";
  let pos = i;
  let found = false;
  for (;;) {
    while (pos < src.length && /[\s]/.test(src[pos])) pos++;
    // skip a comment between adjacent literals
    if (src.startsWith("//", pos)) { pos = src.indexOf("\n", pos); if (pos < 0) return null; continue; }
    if (src.startsWith("/*", pos)) { pos = src.indexOf("*/", pos); if (pos < 0) return null; pos += 2; continue; }
    if (src[pos] === "+" && found) { pos++; continue; }
    const quote = src[pos];
    if (quote !== '"' && !(quote === "'" && found === false && false)) break;
    let j = pos + 1;
    let body = "";
    while (j < src.length && src[j] !== quote) {
      if (src[j] === "\\") { body += src[j] + src[j + 1]; j += 2; } else body += src[j++];
    }
    if (j >= src.length) return null;
    text += unescapeJs(body);
    found = true;
    pos = j + 1;
  }
  return found ? { text, end: pos } : null;
};

const CALLS = [
  { re: /(?<![A-Za-z0-9_.])(?:qsTr|qsTrId)\s*\(/g, kind: "tr" },
  { re: /(?<![A-Za-z0-9_])(?:[A-Za-z0-9_]+::)?tr\s*\(/g, kind: "tr" },
  { re: /(?<![A-Za-z0-9_])(?:QT_TR_NOOP|QT_TRID_NOOP)\s*\(/g, kind: "tr" },
  { re: /(?<![A-Za-z0-9_.])qsTranslate\s*\(\s*"(?:[^"\\]|\\.)*"\s*,/g, kind: "ctx" },
  { re: /(?:QCoreApplication::translate|QT_TRANSLATE_NOOP)\s*\(\s*"(?:[^"\\]|\\.)*"\s*,/g, kind: "ctx" },
];

const extractFrom = (file) => {
  const src = readFileSync(file, "utf8");
  const lines = src.split(/\r?\n/);
  const lineOf = (index) => src.slice(0, index).split("\n").length;
  const found = [];
  const problems = [];
  for (const { re, kind } of CALLS) {
    re.lastIndex = 0;
    for (let m; (m = re.exec(src)); ) {
      const line = lineOf(m.index);
      // a declaration like `QString tr(const char *)` or `static QString tr(` is not a call
      const before = lines[line - 1].slice(0, lines[line - 1].indexOf(m[0]));
      if (/(QString|QCoreApplication|static|virtual|void)\s*$/.test(before) && kind === "tr") continue;
      if (/\/\/\s*$/.test(before) || /^\s*(\*|\/\/)/.test(lines[line - 1])) continue; // inside a comment
      const lit = readLiteral(src, m.index + m[0].length);
      if (lit) { found.push({ text: lit.text, file, line }); continue; }
      const allowed = /aedst-allow:\s*\S/.test(lines[line - 1]) || (line > 1 && /aedst-allow:\s*\S/.test(lines[line - 2]));
      if (!allowed) problems.push({ rule: "i18n-nonliteral", file, line, detail: `${m[0].trim()} has no string literal first argument` });
    }
  }
  return { found, problems };
};

const unxml = (s) =>
  s.replace(/&lt;/g, "<").replace(/&gt;/g, ">").replace(/&quot;/g, '"').replace(/&apos;/g, "'").replace(/&#(\d+);/g, (_m, d) => String.fromCharCode(+d)).replace(/&amp;/g, "&");

const parseCatalog = (file) => {
  const xml = readFileSync(file, "utf8");
  const messages = [];
  for (const m of xml.matchAll(/<message\b([^>]*)>([\s\S]*?)<\/message>/g)) {
    const attrs = m[1];
    const body = m[2];
    const source = /<source>([\s\S]*?)<\/source>/.exec(body);
    const tr = /<translation\b([^>]*?)(?:\/>|>([\s\S]*?)<\/translation>)/.exec(body);
    if (!source) continue;
    const forms = tr?.[2] ? [...tr[2].matchAll(/<numerusform\b[^>]*>([\s\S]*?)<\/numerusform>/g)].map((f) => unxml(f[1])) : [];
    messages.push({
      source: unxml(source[1]),
      numerus: /numerus="yes"/.test(attrs),
      type: /type="([a-z]+)"/.exec(tr?.[1] ?? "")?.[1] ?? "",
      text: forms.length ? "" : unxml((tr?.[2] ?? "").trim()),
      forms,
    });
  }
  return messages;
};

const placeholders = (s) => [...s.matchAll(/%(?:n|[1-9])/g)].map((m) => m[0]).sort().join(",");

const problems = [];
const add = (rule, file, line, detail) => problems.push({ rule, file: relative(root, file).split(sep).join("/"), line, detail });

const modules = findModules();
if (modules.length === 0) {
  console.error(`no i18n/*.ts catalogs under ${root}`);
  process.exit(2);
}
let totalStrings = 0;
for (const mod of modules) {
  const sources = walk(mod.dir).filter((f) => /\.(qml|cpp|hpp|h)$/.test(f));
  const extracted = new Map(); // text -> first location
  if (lupdateRoot) {
    const reference = join(lupdateRoot, relative(root, mod.i18n), `${mod.name}_en.ts`);
    for (const m of parseCatalog(reference)) extracted.set(m.source, { file: reference, line: 0 });
  } else {
    for (const file of sources) {
      const { found, problems: bad } = extractFrom(file);
      for (const p of bad) add(p.rule, p.file, p.line, p.detail);
      for (const f of found) if (!extracted.has(f.text)) extracted.set(f.text, f);
    }
  }
  totalStrings += extracted.size;
  for (const locale of LOCALES) {
    const catalogFile = join(mod.i18n, `${mod.name}_${locale}.ts`);
    let messages;
    try { messages = parseCatalog(catalogFile); } catch { add("i18n-missing", catalogFile, 0, `catalog for ${locale} does not exist`); continue; }
    const bySource = new Map(messages.map((m) => [m.source, m]));
    for (const [text, where] of extracted) {
      const message = bySource.get(text);
      const label = JSON.stringify(text.length > 70 ? `${text.slice(0, 67)}...` : text);
      if (!message) { add("i18n-missing", where.file, where.line, `${label} is not in ${mod.name}_${locale}.ts`); continue; }
      if (message.type || (message.numerus ? message.forms.every((f) => f === "") : message.text === "")) {
        add("i18n-unfinished", catalogFile, 0, `${label} is ${message.type || "empty"} in ${locale}`);
        continue;
      }
      if (text.includes("%n")) {
        const need = MIN_NUMERUS_FORMS[locale];
        if (!message.numerus || message.forms.length < need || message.forms.some((f) => f.trim() === ""))
          add("i18n-numerus", catalogFile, 0, `${label} needs numerus="yes" with >= ${need} filled <numerusform> in ${locale}`);
        else if (message.forms.some((f) => placeholders(f).replace(/%n,?/g, "") !== placeholders(text).replace(/%n,?/g, "")))
          add("i18n-placeholder", catalogFile, 0, `${label}: numerus forms in ${locale} change the %N placeholders`);
      } else if (!message.numerus && placeholders(message.text) !== placeholders(text)) {
        add("i18n-placeholder", catalogFile, 0, `${label}: placeholders differ in ${locale} (${placeholders(text) || "none"} vs ${placeholders(message.text) || "none"})`);
      }
    }
    for (const message of messages) {
      if (!extracted.has(message.source))
        add("i18n-stale", catalogFile, 0, `${JSON.stringify(message.source.slice(0, 70))} no longer occurs in ${basename(mod.dir)} sources`);
    }
  }
}

// --- baseline (shrink-only ratchet for missing/unfinished translations) -------------------------------------------
const BASELINED = new Set(["i18n-missing", "i18n-unfinished"]);
/** Catalog a problem belongs to: the .ts path for catalog problems, else the "<module>_<locale>.ts" named in the text. */
const catalogOf = (p) => (p.file.endsWith(".ts") ? p.file : (p.detail.match(/[a-z0-9]+_[a-z]{2}\.ts/) ?? [p.file])[0]);
const counts = {};
for (const p of problems) {
  if (!BASELINED.has(p.rule)) continue;
  const entry = (counts[basename(catalogOf(p))] ??= { "i18n-missing": 0, "i18n-unfinished": 0 });
  entry[p.rule]++;
}
let baseline = {};
if (baselineFile) { try { baseline = JSON.parse(readFileSync(baselineFile, "utf8")); } catch { baseline = {}; } }
if (updateBaseline) {
  writeFileSync(baselineFile ?? defaultBaseline, JSON.stringify(counts, null, 2) + "\n");
  console.log(`baseline written: ${Object.keys(counts).length} catalogs`);
  process.exit(0);
}
const failures = [];
const stale = [];
for (const key of new Set([...Object.keys(counts), ...Object.keys(baseline)])) {
  for (const rule of BASELINED) {
    const now = counts[key]?.[rule] ?? 0;
    const known = baseline[key]?.[rule] ?? 0;
    if (now > known) failures.push({ key, rule, now, known });
    else if (now < known) stale.push({ key, rule, now, known });
  }
}
// Problems of baselined rules are reported only for catalogs that got worse; every other rule always fails.
const worse = new Set(failures.map((f) => `${f.key}|${f.rule}`));
const reported = problems.filter((p) => !BASELINED.has(p.rule) || worse.has(`${basename(catalogOf(p))}|${p.rule}`));

if (asJson) console.log(JSON.stringify({ modules: modules.length, strings: totalStrings, problems, reported: reported.length, stale }, null, 2));
else {
  const byRule = {};
  for (const p of reported) (byRule[p.rule] ??= []).push(p);
  for (const [rule, list] of Object.entries(byRule)) {
    console.log(`::error::[${rule}] ${list.length} problem(s)`);
    for (const p of list.slice(0, 40)) console.log(`  ${p.file}${p.line ? `:${p.line}` : ""}: ${p.detail}`);
    if (list.length > 40) console.log(`  ... and ${list.length - 40} more (use --json for all)`);
  }
  for (const f of failures) console.log(`::error::[baseline] ${f.key}: ${f.rule} ${f.now} > ${f.known} recorded in the baseline: add the translations, do not raise the baseline`);
  for (const f of stale) console.log(`::error::[baseline] ${f.key}: ${f.rule} is ${f.now} but the baseline still says ${f.known}: run with --update-baseline so the gap can only shrink`);
}
if (reported.length > 0 || failures.length > 0 || stale.length > 0) process.exit(1);
if (!asJson) {
  const debt = Object.values(baseline).reduce((n, e) => n + e["i18n-missing"] + e["i18n-unfinished"], 0);
  console.log(`OK: ${totalStrings} source strings in ${modules.length} modules checked against ${LOCALES.length} catalogs` +
    (debt ? ` (${debt} known missing/unfinished translations recorded in i18n-baseline.json)` : "") + ".");
}
