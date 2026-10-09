#!/usr/bin/env bash
# Design-system gate for QML: feature and app views must compose Aedst* components, not raw Qt Quick Controls.
#
#   scripts/check-qml-components.sh [ROOT]     ROOT defaults to apps/desktop (the self-test passes a fixture root)
#
# Scope: Modules/Features/**/*.qml and App/qml/**/*.qml (not tests/). The design system itself
# (Modules/Core/DesignSystem) wraps the raw controls and is exempt, as is Modules/Core/Media/qml (media primitives).
# Containers and layout types (Item, Rectangle, Column/Row/GridLayout, Flickable, ListView, Repeater, ...) are not
# restricted: only the control types below are, each mapped to the Aedst component that replaces it.
# Escape hatch: `// aedst-allow: <reason>` on the line or the line above (reason mandatory). Comments are ignored.
set -uo pipefail
ROOT="${1:-$(dirname "$0")/..}"
cd "$ROOT" || exit 2

# raw type -> Aedst component (the component file must exist in the design system, or the gate says so)
declare -A REPLACEMENT=(
  [Label]=AedstLabel [Text]=AedstLabel
  [Button]=AedstButton
  [TextField]=AedstTextField [TextArea]=AedstTextArea
  [ItemDelegate]=AedstListItem
  [BusyIndicator]=AedstCircularProgress
  [ComboBox]=AedstComboBox
  [CheckBox]=AedstCheckBox [Switch]=AedstSwitch [RadioButton]=AedstRadioButton
  [Slider]=AedstSlider [ProgressBar]=AedstProgressBar [ToolTip]=AedstToolTip
)
DS=Modules/Core/DesignSystem/qml/AedstDesignSystem

files=$( { find Modules/Features App/qml -type f -name '*.qml' -not -path '*/tests/*' -not -path '*/build/*' 2>/dev/null; } | sort)
[ -z "$files" ] && { echo "no QML files under $PWD"; exit 2; }

types=$(IFS='|'; echo "${!REPLACEMENT[*]}")
status=0
hits=$(echo "$files" | tr '\n' '\0' | xargs -0 awk -v types="$types" '
  FNR == 1 { inblock = 0; prev_allow = 0 }
  {
    line = $0
    allow = (line ~ /aedst-allow:[ \t]*[^ \t]/)
    if (line ~ /aedst-allow:[ \t]*$/) print FILENAME ":" FNR ": aedst-allow needs a reason"
    s = line
    if (inblock) { if (match(s, /\*\//)) { s = substr(s, RSTART + 2); inblock = 0 } else s = "" }
    while (match(s, /\/\*/)) {
      rest = substr(s, RSTART + 2)
      if (match(rest, /\*\//)) s = substr(s, 1, RSTART - 1) " " substr(rest, RSTART + 2)
      else { s = substr(s, 1, RSTART - 1); inblock = 1; break }
    }
    sub(/(^|[^:"])\/\/.*/, "", s)
    if (!(allow || prev_allow)) {
      re = "(^|[^A-Za-z0-9_])([A-Za-z0-9_]+[.])?(" types ")[ \t]*[{]"
      while (match(s, re)) {
        tok = substr(s, RSTART, RLENGTH); gsub(/^[^A-Za-z0-9_]/, "", tok); sub(/[ \t]*[{]$/, "", tok); sub(/^[A-Za-z0-9_]+[.]/, "", tok)
        print FILENAME ":" FNR ": raw " tok
        s = substr(s, RSTART + RLENGTH)
      }
    }
    prev_allow = allow
  }' || true)

if [ -n "$hits" ]; then
  echo "::error::[qml-raw-control] raw Qt Quick Controls in feature/app QML: use the Aedst* component (or add '// aedst-allow: <reason>')"
  while IFS= read -r h; do
    t="${h##* raw }"
    if [[ "$h" == *" raw "* && -n "${REPLACEMENT[$t]:-}" ]]; then
      note="use ${REPLACEMENT[$t]}"; [ -f "$DS/${REPLACEMENT[$t]}.qml" ] || note="$note (MISSING in the design system: add it there first)"
      echo "  $h -> $note"
    else
      echo "  $h"
    fi
  done <<< "$hits"
  status=1
fi

# Every mapped component must exist, otherwise the rule would send people to a file that is not there.
[ -d "$DS" ] && for t in "${!REPLACEMENT[@]}"; do
  [ -f "$DS/${REPLACEMENT[$t]}.qml" ] || { echo "::error::[qml-raw-control] ${REPLACEMENT[$t]}.qml (replacement for $t) is missing in $DS"; status=1; }
done

[ "$status" -eq 0 ] && echo "OK: no raw controls in feature/app QML ($(echo "$files" | wc -l | tr -d ' ') files)."
exit "$status"
