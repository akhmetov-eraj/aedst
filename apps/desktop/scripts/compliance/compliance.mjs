#!/usr/bin/env node
// License and SBOM tooling for the desktop client (docs/compliance.md, ADR-0032). Standard library only.
//
//   node scripts/compliance/compliance.mjs qt                          # Qt modules used vs. the GPL-only list
//   node scripts/compliance/compliance.mjs ffmpeg --dir <install tree> # FFmpeg libraries must not be GPL/nonfree builds
//   node scripts/compliance/compliance.mjs notices --installed <build>/vcpkg_installed/<triplet> --build-dir <build>
//          --out <dir> [--strict]                                      # THIRD_PARTY_NOTICES.txt + sbom.spdx.json
//
// Exit code: 0 = ok, 1 = a policy violation, 2 = the tool could not do its job (missing input).
import { createHash, randomUUID } from 'node:crypto';
import { existsSync, mkdirSync, readdirSync, readFileSync, writeFileSync } from 'node:fs';
import { join, resolve, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..', '..');
const policy = JSON.parse(readFileSync(join(root, 'compliance', 'policy.json'), 'utf8'));
const [command, ...rest] = process.argv.slice(2);
const args = {};
for (let i = 0; i < rest.length; i += 1) {
  if (rest[i].startsWith('--')) args[rest[i].slice(2)] = rest[i + 1] && !rest[i + 1].startsWith('--') ? rest[(i += 1)] : true;
}
const problems = [];
const problem = (text) => {
  problems.push(text);
  console.error(`VIOLATION: ${text}`);
};
const fatal = (text) => {
  console.error(`compliance: ${text}`);
  process.exit(2);
};

function walk(dir, visit, skip = () => false) {
  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    const path = join(dir, entry.name);
    if (entry.isDirectory()) {
      if (!skip(entry.name)) walk(path, visit, skip);
    } else {
      visit(path, entry.name);
    }
  }
}

// ---- qt ------------------------------------------------------------------------------------------------------
function checkQt() {
  const gplOnly = new Set(policy.qtGplOnlyModules.modules);
  const used = new Map(); // module -> first file
  const skip = (name) => name === 'build' || name === 'vcpkg_installed' || name === 'node_modules' || name.startsWith('.');
  walk(root, (path, name) => {
    if (name !== 'CMakeLists.txt' && !name.endsWith('.cmake')) return;
    const text = readFileSync(path, 'utf8');
    for (const call of text.matchAll(/find_package\(\s*Qt6[^)]*\)/g)) {
      const body = call[0].replace(/\$\{[^}]*\}/g, ' ');
      const list = /COMPONENTS\b([^)]*)/.exec(body);
      if (!list) continue;
      for (const token of list[1].split(/\s+/)) {
        if (/^[A-Z][A-Za-z0-9]+$/.test(token) && !['OPTIONAL_COMPONENTS', 'REQUIRED', 'COMPONENTS'].includes(token)) {
          if (!used.has(token)) used.set(token, path.slice(root.length + 1));
        }
      }
    }
    for (const link of text.matchAll(/Qt6::([A-Za-z0-9]+)/g)) {
      if (!used.has(link[1])) used.set(link[1], path.slice(root.length + 1));
    }
  }, skip);
  console.log(`Qt modules in use: ${[...used.keys()].sort().join(', ')}`);
  for (const [module, file] of used) {
    if (!gplOnly.has(module)) continue;
    const waiver = policy.qtGplOnlyModules.waivers[module];
    if (waiver) {
      console.warn(`WAIVED (${module}, first seen in ${file}): ${waiver}`);
    } else {
      problem(`Qt ${module} is GPLv3/commercial only and is used in ${file}`);
    }
  }
}

// ---- ffmpeg --------------------------------------------------------------------------------------------------
function checkFfmpeg() {
  const dir = args.dir ? resolve(args.dir) : fatal('--dir <install tree or Qt directory> is required');
  if (!existsSync(dir)) fatal(`${dir} does not exist`);
  const libs = [];
  walk(dir, (path, name) => {
    if (/^(lib)?(avcodec|avformat|avutil|swresample|swscale)[-.\d]*\.(dll|so)[.\d]*$/i.test(name)) libs.push(path);
  });
  if (libs.length === 0) {
    console.log('No FFmpeg libraries found (another Qt Multimedia backend, or the system FFmpeg). Nothing to check.');
    return;
  }
  const forbidden = ['--enable-gpl', '--enable-nonfree', '--enable-libx264', '--enable-libx265', '--enable-libfdk-aac'];
  for (const lib of libs) {
    // avcodec_configuration() is a string literal inside the library: the build flags are readable in the binary.
    const text = readFileSync(lib).toString('latin1');
    const hits = forbidden.filter((flag) => text.includes(flag));
    const version3 = text.includes('--enable-version3');
    console.log(`${lib.slice(dir.length + 1)}: ${hits.length === 0 ? 'LGPL build' : `FORBIDDEN flags ${hits.join(' ')}`}${version3 ? ' (version3: LGPL-3.0-or-later terms)' : ''}`);
    if (hits.length > 0) problem(`${lib} was built with ${hits.join(', ')}: shipping it makes the product GPL/non-redistributable`);
  }
}

// ---- notices + SBOM --------------------------------------------------------------------------------------------
const tokens = (expression) => expression.split(/[\s()]+/).filter((t) => t && !['AND', 'OR', 'WITH'].includes(t.toUpperCase()));

function licenseProblems(label, expression) {
  if (!expression || expression === 'NOASSERTION' || expression === 'NONE') {
    return [`${label}: the license is not declared (${expression ?? 'missing'})`];
  }
  const out = [];
  for (const id of tokens(expression)) {
    const bare = id.replace(/\+$/, '');
    if (policy.deniedLicensePrefixes.some((p) => bare.startsWith(p) && !bare.startsWith('LGPL'))) out.push(`${label}: license ${id} is denied by compliance/policy.json`);
    else if (!policy.allowedLicenses.includes(bare) && !/^LicenseRef-/.test(bare)) out.push(`${label}: license ${id} is not on the allow list (review, then add it to compliance/policy.json)`);
  }
  return out;
}

const spdxId = (text) => `SPDXRef-${text.replace(/[^A-Za-z0-9.-]+/g, '-')}`;

function generateNotices() {
  let installed = args.installed ? resolve(args.installed) : fatal('--installed <vcpkg_installed[/<triplet>]> is required');
  if (!existsSync(join(installed, 'share')) && existsSync(installed)) {
    // The vcpkg_installed directory itself: take its one triplet directory.
    const triplet = readdirSync(installed, { withFileTypes: true }).find((e) => e.isDirectory() && e.name !== 'vcpkg' && existsSync(join(installed, e.name, 'share')));
    if (triplet) installed = join(installed, triplet.name);
  }
  const buildDir = args['build-dir'] ? resolve(args['build-dir']) : fatal('--build-dir is required');
  const out = args.out ? resolve(args.out) : fatal('--out is required');
  const strict = Boolean(args.strict);
  const share = join(installed, 'share');
  if (!existsSync(share)) fatal(`${share} does not exist (vcpkg install not run?)`);
  const appVersion = /project\([^)]*VERSION\s+([0-9.]+)/s.exec(readFileSync(join(root, 'CMakeLists.txt'), 'utf8'))?.[1] ?? '0.0.0';

  const packages = new Map(); // key -> {name, version, license, homepage, copyright, linking}
  for (const port of readdirSync(share)) {
    const sbom = join(share, port, 'vcpkg.spdx.json');
    if (!existsSync(sbom)) continue;
    const doc = JSON.parse(readFileSync(sbom, 'utf8'));
    const main = (doc.packages ?? [])[0];
    if (!main) continue;
    const license = [main.licenseConcluded, main.licenseDeclared].find((l) => l && l !== 'NOASSERTION') ?? 'NOASSERTION';
    const copyrightFile = join(share, port, 'copyright');
    packages.set(`${main.name}@${main.versionInfo}`, {
      name: main.name,
      version: main.versionInfo ?? 'unknown',
      license,
      homepage: main.homepage ?? main.downloadLocation ?? 'NOASSERTION',
      copyright: existsSync(copyrightFile) ? readFileSync(copyrightFile, 'utf8').trim() : '',
      linking: 'vcpkg',
    });
  }
  if (packages.size === 0) fatal(`no vcpkg.spdx.json found under ${share}`);

  const components = JSON.parse(readFileSync(join(root, 'compliance', 'components.json'), 'utf8')).components;
  for (const c of components) {
    const version = c.version ?? (c.versionFile ? readFileSync(join(root, c.versionFile), 'utf8').trim() : 'unknown');
    let copyright = c.note ?? '';
    if (c.licenseFile) {
      const file = join(buildDir, c.licenseFile);
      if (existsSync(file)) copyright = `${readFileSync(file, 'utf8').trim()}${c.note ? `\n\n${c.note}` : ''}`;
      else if (!c.optional) problem(`${c.name}: license file ${c.licenseFile} not found under ${buildDir}`);
      else {
        console.warn(`optional component ${c.name} skipped: ${c.licenseFile} not present (not built into this configuration)`);
        continue;
      }
    }
    packages.set(`${c.name}@${version}`, { name: c.name, version, license: c.license, homepage: c.homepage, copyright, linking: c.linking });
  }

  const list = [...packages.values()].sort((a, b) => a.name.localeCompare(b.name));
  for (const p of list) {
    const issues = licenseProblems(`${p.name} ${p.version}`, p.license);
    if (strict) issues.forEach(problem);
    else issues.forEach((i) => console.warn(`warning: ${i}`));
    if (tokens(p.license).some((id) => policy.dynamicLinkOnly.includes(id.replace(/\+$/, ''))) && p.linking === 'static') {
      problem(`${p.name}: ${p.license} must be linked dynamically so that users can replace it`);
    }
  }

  mkdirSync(out, { recursive: true });
  const lgpl = readFileSync(join(root, 'compliance', 'licenses', 'LGPL-3.0.txt'), 'utf8');
  const gpl = readFileSync(join(root, 'compliance', 'licenses', 'GPL-3.0.txt'), 'utf8');
  const usesLgpl = list.some((p) => tokens(p.license).some((id) => id.startsWith('LGPL-')));
  const bar = '='.repeat(78);
  let notices = `AEDST Desktop ${appVersion} - third-party software notices\n\nThis product includes the components below under their own licenses.\n`;
  if (usesLgpl) {
    notices += `\nQt and FFmpeg are used under the GNU Lesser General Public License. They are shipped as shared libraries next to the\napplication; you may replace them with your own build of a compatible version. Source code is available from\nhttps://download.qt.io/official_releases/qt/ and https://ffmpeg.org/download.html, or on written request to the publisher.\n`;
  }
  for (const p of list) {
    notices += `\n${bar}\n${p.name} ${p.version}\nLicense: ${p.license}\n${p.homepage}\n${bar}\n${p.copyright}\n`;
  }
  if (usesLgpl) notices += `\n${bar}\nGNU LESSER GENERAL PUBLIC LICENSE, version 3\n${bar}\n${lgpl}\n\n${bar}\nGNU GENERAL PUBLIC LICENSE, version 3 (incorporated by the LGPL)\n${bar}\n${gpl}\n`;
  writeFileSync(join(out, 'THIRD_PARTY_NOTICES.txt'), notices);

  const rootId = 'SPDXRef-aedst-desktop';
  const sbom = {
    spdxVersion: 'SPDX-2.3',
    dataLicense: 'CC0-1.0',
    SPDXID: 'SPDXRef-DOCUMENT',
    name: `aedst-desktop-${appVersion}`,
    documentNamespace: `https://aedst.com/spdx/aedst-desktop-${appVersion}-${randomUUID()}`,
    creationInfo: { created: new Date().toISOString().replace(/\.\d+Z$/, 'Z'), creators: ['Tool: aedst-compliance.mjs'] },
    documentDescribes: [rootId],
    packages: [
      { name: 'aedst-desktop', SPDXID: rootId, versionInfo: appVersion, downloadLocation: 'NOASSERTION', filesAnalyzed: false, licenseConcluded: 'NOASSERTION', licenseDeclared: 'NOASSERTION', copyrightText: 'NOASSERTION' },
      ...list.map((p) => ({
        name: p.name,
        SPDXID: spdxId(`${p.name}-${p.version}`),
        versionInfo: p.version,
        downloadLocation: 'NOASSERTION',
        homepage: p.homepage,
        filesAnalyzed: false,
        licenseConcluded: p.license,
        licenseDeclared: p.license,
        copyrightText: 'NOASSERTION',
      })),
    ],
    relationships: list.map((p) => ({ spdxElementId: rootId, relationshipType: 'DEPENDS_ON', relatedSpdxElement: spdxId(`${p.name}-${p.version}`) })),
  };
  writeFileSync(join(out, 'sbom.spdx.json'), `${JSON.stringify(sbom, null, 2)}\n`);
  const digest = createHash('sha256').update(readFileSync(join(out, 'sbom.spdx.json'))).digest('hex');
  console.log(`wrote ${list.length} components to ${out} (sbom sha256 ${digest.slice(0, 16)}...)`);
}

switch (command) {
  case 'qt':
    checkQt();
    break;
  case 'ffmpeg':
    checkFfmpeg();
    break;
  case 'notices':
    generateNotices();
    break;
  default:
    fatal('usage: compliance.mjs <qt | ffmpeg --dir D | notices --installed D --build-dir D --out D [--strict]>');
}
process.exit(problems.length === 0 ? 0 : 1);
