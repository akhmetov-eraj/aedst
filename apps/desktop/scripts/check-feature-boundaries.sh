#!/usr/bin/env bash
# Fails if any apps/desktop/Modules/Features/<Name>/CMakeLists.txt links
# another Aedst::Feature::* target. See "The boundary rule, translated" in
# docs/architecture-desktop.md — a feature links Core targets only, never
# another Aedst::Feature::* target.
set -euo pipefail

cd "${1:-$(dirname "$0")/..}"

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

# Core never depends on a feature (ARCHITECTURE.md rule 1). This also covers Core/Plugin: the plugin
# contract must stay feature-agnostic.
core_violations=$(grep -rnE 'Aedst::Feature::[A-Za-z0-9_]+' --include=CMakeLists.txt Modules/Core || true)
if [ -n "$core_violations" ]; then
  echo "::error::A Core module references a feature target:"
  echo "$core_violations"
  status=1
fi

# The run-time plugin loader (QPluginLoader) is linked by App/ only: a feature that loads libraries
# from disk would bypass the composition root's trust policy (ADR-0033).
for cmake_file in Modules/Features/*/CMakeLists.txt; do
  if grep -qE 'Aedst::PluginLoader|AedstPluginLoader' "$cmake_file"; then
    echo "::error file=$cmake_file::features must not link the run-time plugin loader (App/ only)"
    status=1
  fi
done

# --- Layer rules inside a feature (Domain <- Data, Presentation) ------------------------------------------------------
# Domain/ is pure business logic: no Qt, and it never includes Data/ or Presentation/.
# Data/ never includes Presentation/. Presentation/ never includes Data/ (it talks to Domain ports; App injects the
# Data implementations). Layout: Modules/Features/<Name>/{Domain,Data,Presentation}. A feature without Domain/ (Media)
# has no port to depend on, but it still must not include its own Data/ from Presentation/.
# Known exceptions are listed in LAYER_EXCEPTIONS as "path|included header|reason" and are printed when they apply.
LAYER_EXCEPTIONS=(
  "Modules/Features/Tasks/Presentation/task_list_view_model.cpp|Data/task_outbox_handlers.hpp|outbox item types and JSON payload builders are the contract between the view model and its outbox handlers; they use QtJson, so they cannot move to Domain/ until a Qt-free JSON writer is approved (owner decision)"
)
layer_check() { # <layer dir> <regex of forbidden include paths> <message>
  local dir="$1" re="$2" msg="$3" hits h file inc exc ok
  hits=$(grep -rnE "^[[:space:]]*#[[:space:]]*include[[:space:]]*[<\"]($re)" --include=*.cpp --include=*.hpp --include=*.h \
           Modules/Features/*/"$dir" 2>/dev/null || true)
  while IFS= read -r h; do
    [ -z "$h" ] && continue
    file="${h%%:*}"
    inc=$(printf '%s' "$h" | sed -E 's/^[^:]*:[0-9]+:[^<"]*[<"]([^>"]*)[>"].*$/\1/')
    ok=0
    for exc in "${LAYER_EXCEPTIONS[@]}"; do
      if [ "${exc%%|*}" = "$file" ] && [ "$(printf '%s' "$exc" | cut -d'|' -f2)" = "$inc" ]; then
        ok=1; echo "note: allowed exception: $file includes $inc ($(printf '%s' "$exc" | cut -d'|' -f3))"
      fi
    done
    if [ "$ok" -eq 0 ]; then echo "::error file=$file::$msg: $h"; status=1; fi
  done <<< "$hits"
}
layer_check Domain       '(Q|Qt)[A-Za-z0-9_/]*[>"]'          "[layer-domain-qt] Domain/ must not include Qt headers"
layer_check Domain       '(\.\./)?(Data|Presentation)/'      "[layer-domain-up] Domain/ must not include Data/ or Presentation/"
layer_check Data         '(\.\./)?Presentation/'             "[layer-data-up] Data/ must not include Presentation/"
layer_check Presentation '(\.\./)?Data/'                     "[layer-presentation-data] Presentation/ must not include Data/ (inject a Domain port)"

if [ "$status" -eq 0 ]; then
  echo "OK: no feature target links another feature target; no Core module links a feature; no feature links the plugin loader; feature layers (Domain/Data/Presentation) respect their include rules."
fi

exit "$status"
