#!/usr/bin/env bash
# The module lists in the docs must match the tree: every directory under Modules/Core and Modules/Features has to be
# named in ARCHITECTURE.md (module graph), and in the module tables of README.md and docs/content/desktop/architecture.md
# (by its `Modules/Core/<Name>` / `Modules/Features/<Name>` path). A module added without a doc row fails CI; a doc row for a
# module that no longer exists fails too.
#
#   scripts/check-docs-modules.sh [ROOT]     ROOT defaults to apps/desktop (the self-test passes a fixture root)
set -uo pipefail
ROOT="${1:-$(cd "$(dirname "$0")/.." && pwd)}"
cd "$ROOT" || exit 2
repo_docs="$ROOT/../../docs/content/desktop/architecture.md"
[ -f "$repo_docs" ] || repo_docs=""

status=0
fail() { echo "::error::[docs-modules] $1"; status=1; }

[ -f ARCHITECTURE.md ] || { echo "::error::[docs-modules] ARCHITECTURE.md not found under $ROOT"; exit 2; }

core=(); features=()
for d in Modules/Core/*/; do [ -d "$d" ] && core+=("$(basename "$d")"); done
for d in Modules/Features/*/; do [ -d "$d" ] && features+=("$(basename "$d")"); done
[ "${#core[@]}" -gt 0 ] || { echo "::error::[docs-modules] no module under Modules/Core"; exit 2; }

# ARCHITECTURE.md: Core modules are lines of the module graph ("     Name   description"); Features are the list in
# "Features/{A,B,...}".
for name in "${core[@]}"; do
  grep -Eq "^[[:space:]│├└─ ]*${name}[[:space:]]+[A-Za-z(]" ARCHITECTURE.md || fail "ARCHITECTURE.md module graph has no line for Modules/Core/${name}"
done
feature_list=$(grep -oE 'Features/\{[^}]*\}' ARCHITECTURE.md | head -1)
for name in "${features[@]}"; do
  grep -Eq "[{,]${name}[,}]" <<<"$feature_list" || fail "ARCHITECTURE.md Features/{...} list does not name ${name}"
done

# Module tables elsewhere: the path must be there.
check_table() {
  local file="$1"
  [ -f "$file" ] || return 0
  for name in "${core[@]}"; do
    [ "$name" = "Testing" ] && [ "$file" = README.md ] && continue
    grep -Fq "Modules/Core/${name}" "$file" || fail "$file does not mention Modules/Core/${name}"
  done
  for name in "${features[@]}"; do
    grep -Eq "Modules/Features/${name}|Features/\{[^}]*\b${name}\b" "$file" || fail "$file does not mention Modules/Features/${name}"
  done
  # A documented path that does not exist is as wrong as a missing one.
  while IFS= read -r path; do
    [ -d "$path" ] || fail "$file documents ${path}, which does not exist"
  done < <(grep -oE 'Modules/(Core|Features)/[A-Za-z]+' "$file" | sort -u)
}
check_table README.md
[ -n "$repo_docs" ] && check_table "$repo_docs"

[ "$status" -eq 0 ] && echo "OK: module lists in the docs match Modules/ (${#core[@]} core, ${#features[@]} features)."
exit "$status"
