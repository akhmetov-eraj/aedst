#!/usr/bin/env bash
# Fails if any apps/desktop/Modules/Features/<Name>/CMakeLists.txt links
# another Aedst::Feature::* target. See "The boundary rule, translated" in
# docs/architecture-desktop.md — a feature links Core targets only, never
# another Aedst::Feature::* target.
set -euo pipefail

cd "$(dirname "$0")/.."

status=0

for cmake_file in Modules/Features/*/CMakeLists.txt; do
  feature_name=$(basename "$(dirname "$cmake_file")")

  # Aedst::Feature::<Other> references anywhere in the file, excluding the
  # feature's own alias declaration and self-reference.
  violations=$(grep -oE 'Aedst::Feature::[A-Za-z0-9_]+' "$cmake_file" \
    | grep -v "^Aedst::Feature::${feature_name}$" || true)

  if [ -n "$violations" ]; then
    echo "::error file=$cmake_file::Modules/Features/$feature_name links another feature target:"
    echo "$violations"
    status=1
  fi
done

if [ "$status" -eq 0 ]; then
  echo "OK: no feature target links another feature target."
fi

exit "$status"
