#!/usr/bin/env node
// Performance budget for the client bundle. Run after `next build`:
//
//   pnpm check:bundle
//
// Sums the gzip size of every emitted client JS and CSS asset under
// `.next/static` and fails when a budget in `bundle-budget.json` is exceeded,
// so a dependency or import that quietly adds 100 KB shows up as a red CI
// check on the pull request that introduced it — not as a Lighthouse
// regression weeks later. (Route-level and Core Web Vitals budgets live in
// `lighthouserc.json`.)
//
// Uses only node built-ins: no new dependency for a 60-line script.
import { readdirSync, readFileSync, statSync } from "node:fs";
import { dirname, extname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";
import { gzipSync } from "node:zlib";

const rootDir = dirname(dirname(fileURLToPath(import.meta.url)));
const staticDir = join(rootDir, ".next", "static");
const budget = JSON.parse(
  readFileSync(join(rootDir, "bundle-budget.json"), "utf8"),
);

function* walk(dir) {
  for (const entry of readdirSync(dir)) {
    const path = join(dir, entry);
    if (statSync(path).isDirectory()) {
      yield* walk(path);
    } else {
      yield path;
    }
  }
}

let assets;
try {
  assets = [...walk(staticDir)]
    .filter((path) => [".js", ".css"].includes(extname(path)))
    .map((path) => ({
      file: relative(staticDir, path).replaceAll("\\", "/"),
      type: extname(path) === ".css" ? "css" : "js",
      gzipBytes: gzipSync(readFileSync(path)).length,
    }));
} catch {
  console.error("No build output found — run `pnpm build` first.");
  process.exit(2);
}

const kb = (bytes) => bytes / 1024;
const sum = (items) => items.reduce((total, item) => total + item.gzipBytes, 0);
const js = assets.filter((asset) => asset.type === "js");
const css = assets.filter((asset) => asset.type === "css");
const largest = [...js].sort((a, b) => b.gzipBytes - a.gzipBytes)[0];

const checks = [
  ["total client JS (gzip)", kb(sum(js)), budget.totalJsGzipKB],
  [
    "largest JS chunk (gzip)",
    kb(largest?.gzipBytes ?? 0),
    budget.largestJsChunkGzipKB,
  ],
  ["total CSS (gzip)", kb(sum(css)), budget.totalCssGzipKB],
];

let failed = false;
console.log("Client bundle budget");
for (const [label, actual, limit] of checks) {
  const ok = actual <= limit;
  failed ||= !ok;
  console.log(
    `  ${ok ? "ok  " : "FAIL"} ${label.padEnd(26)} ${actual.toFixed(1).padStart(7)} KB / ${limit} KB`,
  );
}
if (largest) {
  console.log(`  (largest chunk: ${largest.file})`);
}

if (failed) {
  console.error(
    "\nBudget exceeded. Find what grew (e.g. `ANALYZE` a build or diff the chunk list), " +
      "lazy-load it with next/dynamic, or — if the growth is intended — raise " +
      "bundle-budget.json in the same PR with the reason in the description.",
  );
  process.exit(1);
}
