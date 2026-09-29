#!/usr/bin/env bash
# Fails if any file under src/modules/<name>/** imports from another
# src/modules/<other>/** module. Modules may only communicate through
# src/shared/** (and from src/app/**, which composes them) — see AGENTS.md /
# docs/content/library-first.md. Mirrors the feature-boundary checks on
# Android/Apple/Desktop:
#   apps/android/scripts/check-feature-boundaries.sh
#   apps/apple/scripts/check-feature-boundaries.sh
#   apps/desktop/scripts/check-feature-boundaries.sh
set -euo pipefail

cd "$(dirname "$0")/.."

status=0

for module_dir in src/modules/*/; do
  module_name=$(basename "$module_dir")

  # "@/modules/<other>" alias imports (the only way modules reference each
  # other's code in this codebase — see tsconfig.json "paths"), plus any
  # relative import path that resolves into "modules/<other>/". Reformat
  # each grep hit to "file:line:<target>" so the target (e.g.
  # "@/modules/tasks") can be compared on its own, without the file's own
  # path — which itself contains "modules/<module_name>" — corrupting the
  # comparison.
  while IFS= read -r hit; do
    [ -z "$hit" ] && continue
    file="${hit%%:*}"
    rest="${hit#*:}"
    line_no="${rest%%:*}"
    target="${rest#*:}"
    imported_module="${target##*modules/}"

    if [ "$imported_module" != "$module_name" ]; then
      echo "::error file=$file,line=$line_no::src/modules/$module_name imports from another feature module: $target"
      status=1
    fi
  done < <(grep -rnoE "from [\"'](@/modules/[a-zA-Z0-9_-]+|(\.\./)+modules/[a-zA-Z0-9_-]+)" \
      --include="*.ts" --include="*.tsx" "$module_dir" 2>/dev/null \
    | sed -E "s#^([^:]+):([0-9]+):from [\"']#\1:\2:#" \
    || true)
done

if [ "$status" -eq 0 ]; then
  echo "OK: no feature module imports from another feature module."
fi

exit "$status"
