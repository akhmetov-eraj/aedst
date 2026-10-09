#!/usr/bin/env bash
# Authoritative i18n gate (needs Qt's lupdate; CI lint job): regenerate every module's catalogs with
# `lupdate -no-obsolete` in a TEMPORARY COPY and compare them with the committed ones. Nothing in the tree is
# modified. The comparison itself is scripts/check-i18n-extract.mjs --lupdate-root, i.e. the same rules and the same
# shrink-only baseline as the no-Qt run, but with lupdate (not a regex) deciding which strings exist:
#   - a string lupdate finds that a committed catalog lacks            -> i18n-missing
#   - a committed message lupdate no longer finds (obsolete)           -> i18n-stale
#   - type="unfinished"/vanished/obsolete or empty translations        -> i18n-unfinished
#   - "%n" strings without numerus="yes" + numerusforms in a locale     -> i18n-numerus
# (A textual diff of the .ts files is not used: lupdate rewrites the header, indentation and location lines.)
#
#   scripts/check-i18n-lupdate.sh         QT_ROOT_DIR=<Qt install> or lupdate on PATH
set -euo pipefail
cd "$(dirname "$0")/.."

find_tool() {
  local name="$1" p
  for p in "${QT_ROOT_DIR:-}/bin/$name" "${QT_ROOT_DIR:-}/bin/$name.exe"; do
    [ -x "$p" ] && { echo "$p"; return; }
  done
  command -v "$name" || command -v "$name-qt6" || true
}
lupdate=$(find_tool lupdate)
if [ -z "$lupdate" ]; then
  echo "error: lupdate not found (set QT_ROOT_DIR to the Qt install, or put lupdate on PATH)." >&2
  exit 127
fi

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

# Module = a directory with i18n/<module>_<locale>.ts. Copy it without tests/ and build/ (strings in tests are not shipped).
modules=$(find App Modules -type d -name i18n -not -path '*/build/*' -exec dirname {} \; | sort)
for module in $modules; do
  mkdir -p "$tmp/$module"
  tar -C "$module" --exclude=tests --exclude=build -cf - . | tar -C "$tmp/$module" -xf -
  ts_files=$(find "$tmp/$module/i18n" -name '*.ts' | sort | tr '\n' ' ')
  # shellcheck disable=SC2086
  "$lupdate" -silent -no-obsolete -locations none "$tmp/$module" -ts $ts_files
done

node scripts/check-i18n-extract.mjs --lupdate-root "$tmp"
