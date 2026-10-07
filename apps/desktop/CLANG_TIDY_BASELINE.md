# clang-tidy gate and baseline

clang-tidy is a **blocking** CI check (`desktop-ci.yml`, `lint`-adjacent step in the
`test (clang-debug)` job). It runs `scripts/run-clang-tidy.sh`, which calls
`run-clang-tidy --warnings-as-errors='*'` over `Modules/` and `App/` using the
`clang-debug` preset's `compile_commands.json` (a Clang database, so GCC-only warning
flags cannot produce unknown-option noise) and the checks configured in `.clang-tidy`.

## Enabled checks

`bugprone-*`, `cert-*`, `clang-analyzer-*`, `cppcoreguidelines-*`, `modernize-*`, `performance-*`, `readability-*`,
minus the documented exclusions in `.clang-tidy` (each has a reason next to it). Qt moc/rcc/protobuf output is never
analysed: it lives under `build/**` and `run-clang-tidy.sh` only accepts `Modules/` and `App/`. Phase 2 widened the set
(`cert-*`, full `readability-*`), so the first real run will report more than the previous subset would have.

## The baseline

The tree has never had a clang-tidy run, so some findings will pre-exist. Instead of
running non-blocking forever, known findings are listed in
`scripts/clang-tidy-baseline.txt` as `relative/path:check` pairs (check may be `*`).

- A finding whose `path:check` is in the baseline is ignored.
- Any other finding **fails the job**. New code must be clean; do not add to the baseline.
- A baseline entry that no longer fires is reported as **stale** (a CI warning; set
  `AEDST_TIDY_STRICT_BASELINE=1` to make it fail). Delete stale entries.

## First run (one-time, needs a clang toolchain)

The baseline file is committed **empty**: its true starting contents are unknown until
clang-tidy has run once. Generate it on the first real run and review it in that PR:

```bash
cmake --preset clang-debug && cmake --build --preset clang-debug
./scripts/run-clang-tidy.sh --update-baseline build/clang-debug
```

Until that PR lands, expect the first CI run to fail with the current findings; that
is the honest state, not a regression.

## Shrinking it

Fix a check in one file, delete its line, done. `--update-baseline` is only for the
initial generation and for reviewed, deliberate re-generation after a clang-tidy or
`.clang-tidy` change; a PR that grows the baseline needs an explicit justification.
Goal: the file contains only the header comment.

## Local use

```bash
./scripts/run-clang-tidy.sh build/clang-debug      # same as CI
AEDST_TIDY_LOG=/tmp/tidy.log ./scripts/run-clang-tidy.sh
```

The full raw output is written to `clang-tidy.log` (uploaded as a CI artifact).
