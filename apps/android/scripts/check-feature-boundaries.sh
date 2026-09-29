#!/usr/bin/env bash
# Fails if any apps/android/feature/<name>/build.gradle.kts declares a Gradle
# project dependency on another feature module. See "The boundary rule,
# translated" in docs/architecture-android.md — feature/* may depend on
# core/*, never on another feature/*.
set -euo pipefail

cd "$(dirname "$0")/.."

status=0

for gradle_file in feature/*/build.gradle.kts; do
  feature_name=$(basename "$(dirname "$gradle_file")")

  # project(":feature:<other>") references, excluding a module referencing itself.
  violations=$(grep -oE 'project\(":feature:[a-zA-Z0-9_-]+"\)' "$gradle_file" \
    | grep -v "\":feature:${feature_name}\"" || true)

  if [ -n "$violations" ]; then
    echo "::error file=$gradle_file::feature/$feature_name depends on another feature module:"
    echo "$violations"
    status=1
  fi
done

if [ "$status" -eq 0 ]; then
  echo "OK: no feature module depends on another feature module."
fi

exit "$status"
