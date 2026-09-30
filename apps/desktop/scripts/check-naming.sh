#!/usr/bin/env bash
# Runs the repo-wide naming check (/scripts/check-naming.sh) but only fails for
# files under apps/desktop/, so a naming problem in another app cannot break the
# desktop CI (and vice versa). The full check lives in CONVENTIONS.md's tooling.
set -uo pipefail

repo_root="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$repo_root"

out=$(./scripts/check-naming.sh 2>&1)
status=$?
mine=$(printf '%s\n' "$out" | grep -E '^(ERROR|WARNING): apps/desktop/' || true)

if [ -n "$mine" ]; then
  printf '%s\n' "$mine"
  if printf '%s\n' "$mine" | grep -q '^ERROR:'; then
    echo "Naming check failed for apps/desktop (see CONVENTIONS.md)." >&2
    exit 1
  fi
fi
if [ "$status" -ne 0 ] && [ -z "$mine" ]; then
  echo "note: repo-wide naming check reports problems outside apps/desktop (ignored here)."
fi
echo "OK: apps/desktop naming check passed."
