#!/usr/bin/env bash
# Negative self-tests for the desktop enforcement gates: every gate is run against a fixture that MUST fail (and
# must name the expected rule ids), against a positive-control fixture that MUST pass, and against the real tree.
# A gate that stops failing on its fixture has silently stopped enforcing its rule.
#
#   bash scripts/selftest/run-selftests.sh          from apps/desktop (CI runs it in the lint job)
#
# Fixtures live in scripts/selftest/fixtures/<gate>/ as *.fixture files (so no other tool mistakes them for
# sources); they are copied to a temp dir and the suffix is stripped before a gate sees them.
# Gates that need Qt or clang (qmllint, clang-tidy) are exercised too, but only when the tool is installed.
set -uo pipefail
cd "$(dirname "$0")/../.."
FIX="scripts/selftest/fixtures"
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

failed=0; total=0
report() { # <ok|FAIL> <label> [detail]
  total=$((total + 1)); printf '  %-4s %s%s\n' "$1" "$2" "${3:+  ($3)}"; [ "$1" = ok ] || failed=$((failed + 1))
}

stage() { # <fixture dir name> -> prints the staged root
  local dest="$tmp/$1"; mkdir -p "$dest"
  cp -r "$FIX/$1/." "$dest/"
  find "$dest" -name '*.fixture' | while IFS= read -r f; do mv "$f" "${f%.fixture}"; done
  echo "$dest"
}

# expect_fail <label> <must-contain...> -- <command...>
expect_fail() {
  local label="$1"; shift; local needles=()
  while [ "$1" != "--" ]; do needles+=("$1"); shift; done; shift
  local out status=0
  out=$("$@" 2>&1) || status=$?
  if [ "$status" -eq 0 ]; then report FAIL "$label" "gate PASSED on a fixture that must fail"; return; fi
  local n missing=""
  for n in "${needles[@]}"; do printf '%s' "$out" | grep -qF -- "$n" || missing="$missing [$n]"; done
  if [ -n "$missing" ]; then report FAIL "$label" "failed, but did not report:$missing"; else report ok "$label"; fi
}
expect_pass() { # <label> -- <command...>
  local label="$1"; shift 2; local out
  if out=$("$@" 2>&1); then report ok "$label"; else report FAIL "$label" "gate FAILED where it must pass"; printf '%s\n' "$out" | head -15 | sed 's/^/        /'; fi
}

echo "check-forbidden-apis.sh"
root=$(stage forbidden-apis)
expect_fail "fixture trips every rule" '[ssl-errors]' '[qthread-subclass]' '[std-thread]' '[qnam]' '[bare-qdebug]' \
  '[qml-register]' '[logging-category]' '[secret-storage]' '[logger-in-qt]' '[allow-reason]' '[cmake-package]' \
  '[cmake-fetch-name]' '[cmake-fetch-pin]' -- bash scripts/check-forbidden-apis.sh "$root"
expect_pass "escape hatch and comments are accepted" -- bash scripts/check-forbidden-apis.sh "$(stage forbidden-apis-ok)"
expect_pass "real tree" -- bash scripts/check-forbidden-apis.sh

echo "check-qml-components.sh"
expect_fail "raw controls are reported" 'raw Label' 'raw Text' 'raw TextField' 'raw BusyIndicator' 'raw ComboBox' 'raw ItemDelegate' \
  -- bash scripts/check-qml-components.sh "$(stage qml-components)"
out=$(bash scripts/check-qml-components.sh "$(stage qml-components)" 2>&1 || true)
if printf '%s' "$out" | grep -q 'raw Button'; then report FAIL "allow-marked Button must not be reported"; else report ok "allow-marked Button is accepted"; fi
expect_pass "Aedst components and containers pass" -- bash scripts/check-qml-components.sh "$(stage qml-components-ok)"
expect_pass "real tree" -- bash scripts/check-qml-components.sh

echo "check-qml-accessibility.sh"
expect_fail "missing role/name is reported" 'AedstNoName.qml' 'missing Accessible.name' 'NoRoleNoName.qml' 'Accessible.role Accessible.name' \
  -- bash scripts/check-qml-accessibility.sh "$(stage qml-accessibility)"
expect_pass "role and name present" -- bash scripts/check-qml-accessibility.sh "$(stage qml-accessibility-ok)"
expect_pass "real tree" -- bash scripts/check-qml-accessibility.sh

echo "check-migration-hashes.sh"
d=$(stage migration-edited)
expect_fail "an edited shipped migration is reported" 'was edited after it shipped' \
  -- bash scripts/check-migration-hashes.sh "$d/migrator.cpp" "$d/migrator_test.cpp"
d=$(stage migration-unrecorded)
hash_a=$(node -e 'const fs=require("fs"),c=require("crypto");const s=fs.readFileSync(process.argv[1],"utf8");const m=/R"sql\(([\s\S]*?)\)sql"/.exec(s);console.log(c.createHash("sha256").update(m[1]).digest("hex"))' "$d/migrator.cpp")
sed -i "s/HASH_OF_MIGRATION_A/$hash_a/" "$d/migrator_test.cpp"
expect_fail "a new migration without a recorded hash is reported" 'append the SHA-256' \
  -- bash scripts/check-migration-hashes.sh "$d/migrator.cpp" "$d/migrator_test.cpp"
expect_pass "real tree" -- bash scripts/check-migration-hashes.sh

echo "check-i18n-extract.mjs"
expect_fail "every i18n rule fires" '[i18n-missing]' '[i18n-unfinished]' '[i18n-stale]' '[i18n-numerus]' '[i18n-placeholder]' '[i18n-nonliteral]' \
  -- node scripts/check-i18n-extract.mjs --root "$(stage i18n)"
expect_pass "real tree (shrink-only baseline)" -- node scripts/check-i18n-extract.mjs

echo "check-feature-boundaries.sh"
expect_fail "layer and link violations are reported" '[layer-domain-qt]' '[layer-domain-up]' '[layer-data-up]' '[layer-presentation-data]' \
  'links another feature target' 'A Core module references a feature' 'must not link the run-time plugin loader' \
  -- bash scripts/check-feature-boundaries.sh "$(stage boundaries)"
expect_pass "real tree" -- bash scripts/check-feature-boundaries.sh

echo "check-docs-modules.sh"
expect_fail "a module that no doc names is reported" 'Modules/Core/Ghost' \
  -- bash scripts/check-docs-modules.sh "$(stage docs-modules)"
expect_pass "documented modules pass" -- bash scripts/check-docs-modules.sh "$(stage docs-modules-ok)"
expect_pass "real tree" -- bash scripts/check-docs-modules.sh

echo "check-tracked-sources.sh"
if command -v git >/dev/null; then
  d=$(stage tracked-sources)
  git -C "$d" init -q && git -C "$d" add Modules/Core/Lib/CMakeLists.txt Modules/Core/Lib/src/tracked.cpp
  expect_fail "a CMake-named file that git does not track is reported" 'forgotten.cpp' -- bash scripts/check-tracked-sources.sh "$d"
  git -C "$d" add -A
  expect_pass "once everything is added the gate passes" -- bash scripts/check-tracked-sources.sh "$d"
else
  echo "  skip git not installed"
fi

# --- gates that need a toolchain: exercised only when it is installed -------------------------------------------
echo "toolchain gates (skipped when the tool is missing)"
qmllint=$(command -v qmllint || command -v qmllint-qt6 || { [ -x "${QT_ROOT_DIR:-}/bin/qmllint" ] && echo "$QT_ROOT_DIR/bin/qmllint"; } || true)
if [ -n "$qmllint" ]; then
  printf '%s\n' 'import QtQuick' 'import Aedst.DoesNotExist' 'Item {}' > "$tmp/unresolved.qml"
  expect_fail "qmllint --import error rejects an unresolved import" -- "$qmllint" --import error "$tmp/unresolved.qml"
else
  echo "  skip qmllint not installed"
fi
tidy=$(command -v clang-tidy || true)
if [ -n "$tidy" ]; then
  mkdir -p "$tmp/tidy" && cp .clang-tidy "$tmp/tidy/.clang-tidy"
  printf '%s\n' 'int Bad_function_name(int x) { return x; }' > "$tmp/tidy/naming.cpp"
  expect_fail "clang-tidy: a naming violation is an error (WarningsAsErrors)" 'readability-identifier-naming' \
    -- "$tidy" "$tmp/tidy/naming.cpp" -- -std=c++20
else
  echo "  skip clang-tidy not installed"
fi

echo
if [ "$failed" -ne 0 ]; then echo "::error::$failed of $total self-tests failed"; exit 1; fi
echo "OK: $total self-tests passed."
