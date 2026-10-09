#!/usr/bin/env bash
# Every source file a CMakeLists.txt names must be tracked by git. A file that exists in a working tree but was never
# `git add`ed builds locally and then breaks a clean CI checkout. Run from anywhere; needs a git work tree.
#
#   scripts/check-tracked-sources.sh [ROOT]     (default: apps/desktop)
#
# Paths are taken from tokens ending in a source extension inside each CMakeLists.txt and resolved relative to that file
# (and to the app root for paths such as Modules/...). Tokens containing ${ } or generator expressions are skipped.
set -uo pipefail
ROOT="${1:-$(cd "$(dirname "$0")/.." && pwd)}"
cd "$ROOT" || exit 2
git rev-parse --is-inside-work-tree >/dev/null 2>&1 || { echo "[tracked-sources] not a git work tree: skipped"; exit 0; }

bad=0
checked=0
while IFS= read -r list; do
  dir=$(dirname "$list")
  while IFS= read -r token; do
    case "$token" in *'$'*|*'<'*) continue ;; esac
    for candidate in "$dir/$token" "$token"; do
      if [ -f "$candidate" ]; then
        checked=$((checked + 1))
        if ! git ls-files --error-unmatch -- "$candidate" >/dev/null 2>&1; then
          echo "::error file=$list::[tracked-sources] $candidate is named in CMake but not tracked by git (git add it)"
          bad=1
        fi
        break
      fi
    done
  done < <(tr -d '\r' < "$list" | sed 's/#.*$//' |
           grep -oE '[A-Za-z0-9_./+-]+\.(cpp|hpp|h|cc|qml|js|ts|proto|in|rc|sql|json)\b' | sort -u)
done < <(git ls-files --cached --others --exclude-standard -- 'CMakeLists.txt' '*/CMakeLists.txt' | sort -u)

if [ "$bad" -ne 0 ]; then
  echo "FAILED: some files named in CMake are untracked."
  exit 1
fi
echo "OK: $checked CMake-named source files are tracked."
