#!/usr/bin/env node
// Performance gate for the desktop client: startup time, memory, scroll smoothness and the Qt Test micro-benchmarks, checked
// against perf/budgets.json (absolute budgets) and perf/baseline.json (regression threshold per micro-benchmark).
//
//   node scripts/perf/run-perf.mjs --build-dir build/msvc-release [--startup-runs 7] [--only startup,scroll,micro]
//                                  [--update-baseline] [--report perf-report.json]
//
// Exit code 0 = all budgets met, 1 = a budget or regression failed, 2 = the harness itself failed (a missing executable,
// an app that never reached its first frame). Standard library only.
//
// What each part measures (see docs/performance.md):
//   startup  cold-ish launches of aedst-desktop against an empty profile directory; reads the `startup +N ms: <phase>`
//            lines the app writes to its log, takes the MEDIAN over --startup-runs, and the resident memory 3 s after the
//            first frame.
//   scroll   Modules/Core/Media/bench/scroll_bench.cpp (frame-time percentiles and RSS of a 5 000-tile media grid).
//   micro    Qt Test QBENCHMARKs (AedstNetworkBench); each result must stay within the regression threshold of the baseline
//            recorded on the same kind of runner. A benchmark with no baseline entry is recorded and passes (first run).
import { spawn, spawnSync } from 'node:child_process';
import { existsSync, mkdirSync, mkdtempSync, readdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const root = resolve(here, '..', '..');
const isWindows = process.platform === 'win32';

const args = parseArgs(process.argv.slice(2));
const buildDir = resolve(root, args['build-dir'] ?? '');
if (!args['build-dir'] || !existsSync(buildDir)) fatal('--build-dir <existing build tree> is required');
const budgets = JSON.parse(readFileSync(join(root, 'perf', 'budgets.json'), 'utf8'));
const baselinePath = join(root, 'perf', 'baseline.json');
// { "win32": { "<test>:<tag>": ms }, "linux": { ... } }: timings are only comparable on the same kind of runner.
const baseline = existsSync(baselinePath) ? JSON.parse(readFileSync(baselinePath, 'utf8')) : {};
const only = new Set((args.only ?? 'startup,scroll,micro').split(','));
const report = { platform: process.platform, startup: null, scroll: null, micro: {}, failures: [] };

function parseArgs(list) {
  const out = {};
  for (let i = 0; i < list.length; i += 1) {
    if (!list[i].startsWith('--')) continue;
    const key = list[i].slice(2);
    out[key] = list[i + 1] && !list[i + 1].startsWith('--') ? list[(i += 1)] : true;
  }
  return out;
}

function fatal(message) {
  console.error(`run-perf: ${message}`);
  process.exit(2);
}

function fail(message) {
  report.failures.push(message);
  console.error(`FAIL: ${message}`);
}

function findExecutable(name) {
  const file = isWindows ? `${name}.exe` : name;
  const stack = [buildDir];
  while (stack.length > 0) {
    const dir = stack.pop();
    for (const entry of readdirSync(dir, { withFileTypes: true })) {
      const path = join(dir, entry.name);
      if (entry.isDirectory()) {
        if (!entry.name.startsWith('CMakeFiles') && entry.name !== 'vcpkg_installed') stack.push(path);
      } else if (entry.name === file) {
        return path;
      }
    }
  }
  return fatal(`executable ${file} not found under ${buildDir} (built with -DAEDST_BUILD_BENCHMARKS=ON?)`);
}

const median = (values) => {
  const sorted = [...values].sort((a, b) => a - b);
  const mid = Math.floor(sorted.length / 2);
  return sorted.length % 2 ? sorted[mid] : (sorted[mid - 1] + sorted[mid]) / 2;
};

function residentMiB(pid) {
  try {
    if (isWindows) {
      const out = spawnSync('powershell', ['-NoProfile', '-Command', `(Get-Process -Id ${pid}).WorkingSet64`], { encoding: 'utf8' });
      return Number(out.stdout.trim()) / (1024 * 1024);
    }
    const status = readFileSync(`/proc/${pid}/status`, 'utf8');
    const kb = /VmRSS:\s+(\d+)/.exec(status);
    return kb ? Number(kb[1]) / 1024 : NaN;
  } catch {
    return NaN;
  }
}

const sleep = (ms) => new Promise((done) => setTimeout(done, ms));

// ---- startup ---------------------------------------------------------------------------------------------------
async function launchOnce(exe) {
  const profile = mkdtempSync(join(tmpdir(), 'aedst-perf-'));
  const env = {
    ...process.env,
    // An empty profile every time: no cache, no settings, no database.
    XDG_DATA_HOME: join(profile, 'data'),
    XDG_CONFIG_HOME: join(profile, 'config'),
    XDG_CACHE_HOME: join(profile, 'cache'),
    APPDATA: join(profile, 'roaming'),
    LOCALAPPDATA: join(profile, 'local'),
    AEDST_API_BASE_URL: 'https://localhost:9', // never reached: the login screen is the first frame
    AEDST_SENTRY_DSN: '',
    AEDST_POSTHOG_KEY: '',
    AEDST_UPDATE_MANIFEST_URL: '',
    QT_QUICK_CONTROLS_STYLE: 'Basic',
  };
  if (!isWindows && !process.env.DISPLAY && !process.env.WAYLAND_DISPLAY) env.QT_QPA_PLATFORM = 'offscreen';
  const child = spawn(exe, [], { env, stdio: 'ignore' });
  const logRoots = [join(profile, 'data'), join(profile, 'roaming'), join(profile, 'local')];
  const readLog = () => {
    const stack = [...logRoots];
    while (stack.length > 0) {
      const dir = stack.pop();
      if (!existsSync(dir)) continue;
      for (const entry of readdirSync(dir, { withFileTypes: true })) {
        const path = join(dir, entry.name);
        if (entry.isDirectory()) stack.push(path);
        else if (entry.name === 'aedst.log') return readFileSync(path, 'utf8');
      }
    }
    return '';
  };
  const phase = (log, name) => {
    const hit = new RegExp(`startup \\+(\\d+) ms: ${name}`).exec(log);
    return hit ? Number(hit[1]) : null;
  };
  let log = '';
  const deadline = Date.now() + 60_000;
  while (Date.now() < deadline && phase(log, 'first frame') === null) {
    await sleep(200);
    log = readLog();
  }
  const result = { firstFrameMs: phase(log, 'first frame'), qmlLoadedMs: phase(log, 'QML loaded') };
  if (result.firstFrameMs !== null) {
    await sleep(3000);
    result.rssMiB = residentMiB(child.pid);
  }
  child.kill();
  await sleep(300);
  rmSync(profile, { recursive: true, force: true, maxRetries: 5 });
  return result;
}

async function runStartup() {
  const exe = findExecutable('aedst-desktop');
  const runs = Number(args['startup-runs'] ?? 7);
  const samples = [];
  for (let i = 0; i < runs; i += 1) {
    const sample = await launchOnce(exe);
    if (sample.firstFrameMs === null) fatal('aedst-desktop never logged "startup ... first frame" within 60 s');
    samples.push(sample);
    console.log(`startup run ${i + 1}/${runs}: first frame ${sample.firstFrameMs} ms, ${sample.rssMiB?.toFixed(0)} MiB`);
  }
  const result = {
    runs,
    firstFrameMedianMs: median(samples.map((s) => s.firstFrameMs)),
    qmlLoadedMedianMs: median(samples.map((s) => s.qmlLoadedMs ?? 0)),
    rssMedianMiB: median(samples.map((s) => s.rssMiB).filter(Number.isFinite)),
  };
  report.startup = result;
  const limit = budgets.startup;
  if (result.firstFrameMedianMs > limit.firstFrameMedianMs) fail(`startup first frame ${result.firstFrameMedianMs} ms > ${limit.firstFrameMedianMs} ms`);
  if (result.qmlLoadedMedianMs > limit.qmlLoadedMedianMs) fail(`startup QML loaded ${result.qmlLoadedMedianMs} ms > ${limit.qmlLoadedMedianMs} ms`);
  if (result.rssMedianMiB > limit.rssMiB) fail(`startup memory ${result.rssMedianMiB.toFixed(0)} MiB > ${limit.rssMiB} MiB`);
}

// ---- scroll ----------------------------------------------------------------------------------------------------
function runScroll() {
  const exe = findExecutable('AedstMediaScrollBench');
  const limit = budgets.scroll;
  const out = spawnSync(exe, [`--budget-p95-ms=${limit.p95Ms}`, `--budget-p99-ms=${limit.p99Ms}`, `--budget-rss-mib=${limit.rssMiB}`], {
    encoding: 'utf8',
    env: { ...process.env, QT_QUICK_CONTROLS_STYLE: 'Basic' },
    timeout: 180_000,
  });
  const line = out.stdout.split('\n').find((l) => l.startsWith('{'));
  if (!line) fatal(`the scroll benchmark produced no result (exit ${out.status}): ${out.stderr}`);
  report.scroll = JSON.parse(line);
  console.log(`scroll: ${line}`);
  if (out.status === 1) fail(`scroll budget exceeded: ${out.stderr.trim()}`);
  else if (out.status !== 0) fatal(`the scroll benchmark failed to run (exit ${out.status}): ${out.stderr}`);
}

// ---- micro-benchmarks (Qt Test) --------------------------------------------------------------------------------
function parseQtTestXml(xml) {
  const results = {};
  for (const fn of xml.matchAll(/<TestFunction name="([^"]+)">([\s\S]*?)<\/TestFunction>/g)) {
    for (const r of fn[2].matchAll(/<BenchmarkResult metric="WalltimeMilliseconds" tag="([^"]*)" value="([^"]+)"/g)) {
      results[`${fn[1]}${r[1] ? `:${r[1]}` : ''}`] = Number(r[2]);
    }
  }
  return results;
}

function runMicro() {
  const exe = findExecutable('AedstNetworkBench');
  const xmlPath = join(mkdtempSync(join(tmpdir(), 'aedst-bench-')), 'bench.xml');
  const out = spawnSync(exe, ['-o', `${xmlPath},xml`], { encoding: 'utf8', timeout: 600_000 });
  if (!existsSync(xmlPath)) fatal(`the micro-benchmarks produced no result (exit ${out.status}): ${out.stderr}`);
  const current = parseQtTestXml(readFileSync(xmlPath, 'utf8'));
  report.micro = current;
  const threshold = 1 + budgets.regressionThresholdPercent / 100;
  for (const [name, value] of Object.entries(current)) {
    const known = (baseline[process.platform] ?? {})[name];
    if (known === undefined) {
      console.log(`micro ${name}: ${value} ms (no baseline yet - recorded, not checked)`);
    } else if (value > known * threshold) {
      fail(`micro ${name}: ${value} ms is more than ${budgets.regressionThresholdPercent}% above the baseline ${known} ms`);
    } else {
      console.log(`micro ${name}: ${value} ms (baseline ${known} ms)`);
    }
  }
  if (args['update-baseline']) {
    mkdirSync(dirname(baselinePath), { recursive: true });
    writeFileSync(baselinePath, `${JSON.stringify({ ...baseline, [process.platform]: current }, null, 2)}\n`);
    console.log(`baseline written to ${baselinePath}`);
  }
}

if (only.has('startup')) await runStartup();
if (only.has('scroll')) runScroll();
if (only.has('micro')) runMicro();
if (args.report) writeFileSync(resolve(args.report), `${JSON.stringify(report, null, 2)}\n`);
process.exit(report.failures.length === 0 ? 0 : 1);
