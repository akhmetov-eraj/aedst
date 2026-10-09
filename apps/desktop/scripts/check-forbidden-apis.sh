#!/usr/bin/env bash
# Forbidden-API gate for first-party C++ (Modules/, App/, Tests/) and the CMake files.
# One rule = one id in [brackets]; every id has a fixture under scripts/selftest/ that MUST fail
# (scripts/selftest/run-selftests.sh). Rules, owners and rationale: docs/content/library-first/desktop.md
# ("Enforced rules"), ADR-0004 (TLS), ADR-0024 (logging), docs/performance.md (QML registration).
#
#   scripts/check-forbidden-apis.sh [ROOT]     ROOT defaults to apps/desktop (fixtures pass their own root)
#
# Escape hatch: put `// aedst-allow: <reason>` on the offending line or the line above it. The reason is
# mandatory (a bare marker is itself an error). Comments are stripped before matching, so a banned name
# in a comment does not trip a rule. Pure bash/awk/node: runs without Qt, vcpkg or a compiler.
set -uo pipefail
ROOT="${1:-$(dirname "$0")/..}"
cd "$ROOT" || exit 2

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
code="$tmp/code.txt"   # "path:line:code" with comments stripped and allow-marked lines dropped
status=0
fail() { echo "::error::[$1] $2"; shift 2; printf '%s\n' "$@" | sed 's/^/  /'; status=1; }

files=$( { find Modules App Tests -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) \
             -not -path '*/build/*' -not -path '*/generated/*' 2>/dev/null; } | sort)
[ -z "$files" ] && { echo "no C++ sources under $PWD"; exit 2; }

# --- strip comments (strings respected), drop lines carrying a valid allow marker --------------------------
echo "$files" | tr '\n' '\0' | xargs -0 awk '
function strip(s,   out, i, c, n, instr, q) {
  out = ""; n = length(s); instr = 0
  for (i = 1; i <= n; i++) {
    c = substr(s, i, 1)
    if (inblock) { if (c == "*" && substr(s, i + 1, 1) == "/") { inblock = 0; i++ } ; continue }
    if (instr) { out = out c; if (c == "\\") { out = out substr(s, i + 1, 1); i++ } else if (c == q) instr = 0; continue }
    if (c == "\"" || c == "\x27") { instr = 1; q = c; out = out c; continue }
    if (c == "/" && substr(s, i + 1, 1) == "/") break
    if (c == "/" && substr(s, i + 1, 1) == "*") { inblock = 1; i++; continue }
    out = out c
  }
  return out
}
FNR == 1 { inblock = 0; prev_allow = 0 }
{
  line = $0
  allow = (line ~ /aedst-allow:[ \t]*[^ \t]/)
  if (line ~ /aedst-allow:[ \t]*$/) print FILENAME ":" FNR ":@@BARE-ALLOW@@"
  s = strip(line)
  if (!(allow || prev_allow)) print FILENAME ":" FNR ":" s
  prev_allow = allow
}' > "$code"

bare=$(grep -F '@@BARE-ALLOW@@' "$code" || true)
[ -n "$bare" ] && fail allow-reason "\`aedst-allow:\` needs a reason" "$bare"
grep -vF '@@BARE-ALLOW@@' "$code" > "$code.2"; mv "$code.2" "$code"

is_test='(^|/)(tests|Tests|bench)/'
# scan <id> <message> <regex> [exclude-path-regex]   (matches against the stripped code)
scan() {
  local id="$1" msg="$2" re="$3" ex="${4:-}" hits
  hits=$(grep -E "^[^:]+:[0-9]+:.*($re)" "$code" || true)
  [ -n "$ex" ] && hits=$(printf '%s\n' "$hits" | grep -Ev "^($ex)" || true)
  [ -n "$(printf '%s' "$hits")" ] && fail "$id" "$msg" "$hits"
  return 0
}

scan ssl-errors "ignoreSslErrors is forbidden (ADR-0004): pin or trust the system store instead" \
  'ignoreSslErrors'
scan qthread-subclass "do not subclass QThread: use a QObject worker + moveToThread or QtConcurrent/QThreadPool" \
  ':[[:space:]]*(public|protected|private)?[[:space:]]*QThread\b'
scan std-thread "std::thread/jthread only in Modules/Core/Persistence (the DB thread, ADR-0028); use QThreadPool/QtConcurrent" \
  'std::(thread|jthread)\b' "Modules/Core/Persistence/|[^:]*($is_test)"
scan qnam "QNetworkAccessManager only in Core/{Network,Oidc,Media,Telemetry,Update,Updater,Testing}; features use ConnectClient" \
  '\bQNetworkAccessManager\b' "Modules/Core/(Network|Oidc|Media|Telemetry|Update|Updater|Testing)/|App/src/app_dependencies\.|[^:]*($is_test)"
scan bare-qdebug "bare qDebug()/qWarning()/qInfo()/qCritical(): use a category (qCDebug(lcX), Aedst::Logging)" \
  '\bq(Debug|Warning|Info|Critical)[[:space:]]*\('
scan qml-register "qmlRegister*: use QML_ELEMENT/QML_SINGLETON + static create() (docs/performance.md)" \
  '\bqmlRegister[A-Za-z]*[[:space:]]*[<(]' "[^:]*($is_test)"
scan logging-category "QLoggingCategory declarations live in Modules/Core/Logging (Aedst::Logging) only" \
  'Q_(DECLARE_)?LOGGING_CATEGORY[[:space:]]*\(|\bQLoggingCategory[[:space:]]+[A-Za-z_]+[[:space:]]*[({;=]' \
  "Modules/Core/Logging/|[^:]*($is_test)"

# --- file-set rules (one grep/awk pass each: process spawns are slow on Windows) ---------------------------
nontest=$(echo "$files" | grep -Ev "$is_test")
# in_set <list-file> <code-regex>: lines of code.txt from files in the list that match the regex
in_set() { awk -F: -v re="$2" 'NR==FNR { set[$0] = 1; next } ($1 in set) && substr($0, length($1) + length($2) + 3) ~ re' "$1" "$code"; }

# secrets: QSettings/QFile/QSaveFile in a file that handles tokens, passwords or secrets
echo "$nontest" | tr '\n' '\0' | xargs -0 grep -liE 'token|password|secret' > "$tmp/secretish.txt" 2>/dev/null
sec=$(in_set "$tmp/secretish.txt" '(^|[^A-Za-z_])(QSettings|QFile|QSaveFile)([^A-Za-z_]|$)' | grep -vE ':[0-9]+:[[:space:]]*#[[:space:]]*include' || true)
[ -n "$sec" ] && fail secret-storage "QSettings/QFile/QSaveFile in a file that mentions token|password|secret: secrets go through Core/Platform secure storage or QtKeychain (ADR-0003); mark non-secret uses with aedst-allow" "$sec"

# logging front doors (ADR-0024): Qt-aware code logs through categories
echo "$nontest" | grep -v '^Modules/Core/Common/' | tr '\n' '\0' | xargs -0 grep -lE '^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"]Q' > "$tmp/qtaware.txt" 2>/dev/null
lg=$(in_set "$tmp/qtaware.txt" '(^|[^A-Za-z_])AedstLogger::' || true)
[ -n "$lg" ] && fail logger-in-qt "AedstLogger in a file that includes Qt headers: use a QLoggingCategory (ADR-0024; Qt-free layers keep AedstLogger)" "$lg"

# --- CMake: dependency names and pins -----------------------------------------------------------------------
node - <<'JS' || status=1
const fs = require("fs"), path = require("path");
const walk = (d, o = []) => { for (const e of fs.readdirSync(d, { withFileTypes: true })) {
  if (["build", "third_party", "node_modules", "selftest"].includes(e.name)) continue;
  const p = path.join(d, e.name);
  if (e.isDirectory()) walk(p, o); else if (e.name === "CMakeLists.txt" || e.name.endsWith(".cmake")) o.push(p); } return o; };
const read = (f) => (fs.existsSync(f) ? JSON.parse(fs.readFileSync(f, "utf8")) : {});
const vcpkg = (read("vcpkg.json").dependencies || []).map((d) => (typeof d === "string" ? d : d.name));
for (const f of Object.values(read("vcpkg.json").features || {})) for (const d of f.dependencies || []) vcpkg.push(typeof d === "string" ? d : d.name);
const comps = (read("compliance/components.json").components || []).map((c) => c.name.split(" ")[0]);
// find_package spelling -> package name it is declared under (vcpkg port, Qt, or CMake built-in module).
const alias = { gtest: "gtest", sqlite3: "sqlite3", sentry: "sentry-native", qt6keychain: "qtkeychain" };
const builtin = new Set(["threads", "pkgconfig", "qt6"]);
const known = new Set([...vcpkg, ...comps].map((s) => s.toLowerCase()));
let bad = 0;
const err = (id, f, msg) => { console.log(`::error file=${f}::[${id}] ${msg}`); bad = 1; };
for (const f of walk(".")) {
  const text = fs.readFileSync(f, "utf8").split("\n").map((l) => l.replace(/(^|\s)#.*/, "")).join("\n");
  const allowed = (i) => /aedst-allow:\s*\S/.test(text.split("\n").slice(Math.max(0, text.slice(0, i).split("\n").length - 2), text.slice(0, i).split("\n").length).join("\n"));
  for (const m of text.matchAll(/\bfind_package\s*\(\s*([A-Za-z0-9_+-]+)/gi)) {
    const n = m[1].toLowerCase(), real = alias[n] || n;
    if (!builtin.has(n) && !known.has(real) && !allowed(m.index))
      err("cmake-package", f, `find_package(${m[1]}) is not in vcpkg.json, Qt6 or compliance/components.json: add it there (and to library-first/desktop.md) first`);
  }
  for (const m of text.matchAll(/\bFetchContent_Declare\s*\(([^)]*)\)/gi)) {
    const body = m[1], name = body.trim().split(/\s+/)[0], line = text.slice(0, m.index).split("\n").length;
    if (!known.has(name.toLowerCase())) err("cmake-fetch-name", `${f}:${line}`, `FetchContent_Declare(${name}) is not in compliance/components.json`);
    const tag = /\bGIT_TAG\s+(\S+)/.exec(body);
    if (tag && !/^[0-9a-f]{40}$/.test(tag[1])) err("cmake-fetch-pin", `${f}:${line}`, `FetchContent_Declare(${name}) GIT_TAG ${tag[1]} must be a full 40-hex commit (tags move)`);
    if (/\bURL\s/.test(body) && !/\bURL_HASH\b/.test(body)) err("cmake-fetch-pin", `${f}:${line}`, `FetchContent_Declare(${name}) has URL without URL_HASH`);
    if (!tag && !/\bURL\s/.test(body)) err("cmake-fetch-pin", `${f}:${line}`, `FetchContent_Declare(${name}) has neither a pinned GIT_TAG nor URL+URL_HASH`);
  }
}
process.exit(bad);
JS

[ "$status" -eq 0 ] && echo "OK: forbidden-API check passed ($(echo "$files" | wc -l | tr -d ' ') C++ files)."
exit "$status"
