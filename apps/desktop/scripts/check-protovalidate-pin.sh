#!/usr/bin/env bash
# Fails if third_party/protovalidate/buf/validate/validate.proto differs from what
# `buf export` returns for the protovalidate commit pinned in apps/server/buf.lock.
# Needs `buf` and network access to buf.build; skip (exit 0) when buf is missing.
set -euo pipefail

cd "$(dirname "$0")/.."

if ! command -v buf >/dev/null 2>&1; then
  echo "::warning::buf not installed; skipping protovalidate pin check."
  exit 0
fi

lock=../server/buf.lock
commit=$(awk '/name: buf.build\/bufbuild\/protovalidate/ {found=1; next} found && /commit:/ {print $2; exit}' "$lock")
if [ -z "$commit" ]; then
  echo "::error::could not read the protovalidate commit from $lock"
  exit 1
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
buf export "buf.build/bufbuild/protovalidate:$commit" -o "$tmp"

if diff --strip-trailing-cr -q "$tmp/buf/validate/validate.proto" third_party/protovalidate/buf/validate/validate.proto >/dev/null; then
  echo "OK: vendored validate.proto matches protovalidate@$commit."
else
  echo "::error::third_party/protovalidate is stale vs apps/server/buf.lock (protovalidate@$commit). See third_party/protovalidate/README.md."
  exit 1
fi
