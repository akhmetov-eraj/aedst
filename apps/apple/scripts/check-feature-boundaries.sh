#!/usr/bin/env bash
# Fails if any apps/apple/Features/<Name>/Package.swift declares a local
# package dependency on another Features/* package. See "The boundary rule,
# translated" in docs/architecture-apple.md — Features/* may depend on
# Core/*, never on another Features/* package.
set -euo pipefail

cd "$(dirname "$0")/.."

status=0

for package_file in Features/*/Package.swift; do
  feature_name=$(basename "$(dirname "$package_file")")

  # .package(path: "../../Features/<Other>") references, excluding self-reference.
  violations=$(grep -oE '\.package\(path: *"[^"]*Features/[A-Za-z0-9_-]+"\)' "$package_file" \
    | grep -v "Features/${feature_name}\"" || true)

  if [ -n "$violations" ]; then
    echo "::error file=$package_file::Features/$feature_name depends on another feature package:"
    echo "$violations"
    status=1
  fi
done

if [ "$status" -eq 0 ]; then
  echo "OK: no feature package depends on another feature package."
fi

exit "$status"
