#!/usr/bin/env bash
# Checks file/resource naming conventions that per-platform linters don't cover
# (file names, directories) — see /CONVENTIONS.md for the full standard.
#
# Usage:
#   ./scripts/check-naming.sh            # check all tracked files
#   ./scripts/check-naming.sh --staged   # check only staged files (pre-commit)
set -euo pipefail

fail=0
warn() { echo "WARNING: $*"; }
err()  { echo "ERROR: $*"; fail=1; }

if [[ "${1:-}" == "--staged" ]]; then
  files=$(git diff --cached --name-only --diff-filter=ACMR)
else
  files=$(git ls-files)
fi

kebab='^[a-z0-9]+(-[a-z0-9]+)*$'
pascal='^[A-Z][a-zA-Z0-9]*$'
snake='^[a-z0-9]+(_[a-z0-9]+)*$'

while IFS= read -r f; do
  [[ -z "$f" ]] && continue
  [[ -f "$f" ]] || continue  # skip files deleted-but-not-yet-committed
  base=$(basename -- "$f")
  name="${base%.*}"
  name="${name%.d}" # `foo.d.ts` declaration files: the stem is `foo`
  case "$f" in apps/web/*) name="${name%.test}"; name="${name%.spec}"; name="${name%.contract}" ;; esac # `foo.contract.test.ts`: the stem is `foo`

  case "$f" in
    # --- Generated code: naming is owned by the generator, not us ---
    apps/server/gen/*)
      ;;

    # --- Web (apps/web) ---
    apps/web/src/shared/ui/*)
      ;; # shadcn/ui-generated primitives: documented kebab-case exception
    apps/web/src/app/*/page.tsx|apps/web/src/app/*/layout.tsx|apps/web/src/app/*/loading.tsx|apps/web/src/app/*/error.tsx|apps/web/src/app/*/not-found.tsx|apps/web/src/app/*/route.ts|apps/web/src/app/*/global-error.tsx)
      ;; # Next.js framework-mandated literal file names
    apps/web/src/modules/*/ui/*.tsx)
      name="${name%.test}" # <Component>.test.tsx sits next to its component
      [[ "$name" =~ $pascal ]] || err "$f: component file must be PascalCase.tsx"
      ;;
    apps/web/src/*.ts)
      [[ "$name" =~ $kebab ]] || warn "$f: utility file should be kebab-case.ts"
      ;;

    # --- Server (apps/server, Go) ---
    apps/server/*.go)
      [[ "$name" =~ $snake ]] || err "$f: Go file must be snake_case.go (or one lowercase word)"
      ;;

    # --- Android (apps/android, Kotlin) ---
    apps/android/*.kt)
      [[ "$name" =~ $pascal ]] || err "$f: Kotlin file must be PascalCase.kt"
      ;;
    apps/android/*/res/*.xml)
      [[ "$name" =~ $snake ]] || err "$f: Android resource must be snake_case"
      ;;

    # --- Apple (apps/apple, Swift) ---
    apps/apple/*.swift)
      [[ "$name" =~ $pascal ]] || err "$f: Swift file must be PascalCase.swift"
      ;;

    # --- Desktop (apps/desktop, C++/Qt) ---
    apps/desktop/*.cpp|apps/desktop/*.hpp|apps/desktop/*.h)
      [[ "$name" =~ $snake ]] || err "$f: C++ file must be snake_case.cpp/.hpp/.h (see CONVENTIONS.md)"
      ;;
    apps/desktop/*.ui)
      [[ "$name" =~ $snake ]] || err "$f: Qt .ui file must be snake_case"
      ;;
  esac
done <<< "$files"

if [[ $fail -ne 0 ]]; then
  echo ""
  echo "Naming check failed. See CONVENTIONS.md for the standard." >&2
  exit 1
fi
echo "Naming check passed."
