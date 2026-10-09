#!/usr/bin/env bash
# Accessibility gate for QML: a file that declares an interactive control must declare its accessible role AND name.
#
#   scripts/check-qml-accessibility.sh [ROOT]     ROOT defaults to apps/desktop (the self-test passes a fixture root)
#
# Interactive types: Button, TextField, TextArea, ItemDelegate, MouseArea, ComboBox, Switch, CheckBox, RadioButton,
# Slider, SpinBox, ToolButton, TabButton (bare or qualified: T.Button, Controls.Button). Aedst* components are not
# matched by name: the design-system file that wraps a raw control is where the role/name must live.
# Rule (file level, deliberately cheap): such a file must contain both `Accessible.role` and `Accessible.name`
# (a binding like `Accessible.name: text` counts). Feature views that only compose Aedst* components pass trivially;
# the gate bites in the design system and in any file that sneaks in a raw control or a MouseArea.
# Scope: all QML under Modules/ and App/ except tests/ and the generated AedstTokens.qml.
# Escape hatch: `// aedst-allow: <reason>` anywhere in the file on the control line or the line above (reason mandatory).
set -uo pipefail
ROOT="${1:-$(dirname "$0")/..}"
cd "$ROOT" || exit 2

files=$( { find Modules App -type f -name '*.qml' -not -path '*/tests/*' -not -path '*/build/*' \
             -not -name 'AedstTokens.qml' 2>/dev/null; } | sort)
[ -z "$files" ] && { echo "no QML files under $PWD"; exit 2; }

status=0
out=$(echo "$files" | tr '\n' '\0' | xargs -0 awk '
  function flush(   miss) {
    if (file != "" && nctl > 0) {
      miss = ""
      if (!hasrole) miss = miss " Accessible.role"
      if (!hasname) miss = miss " Accessible.name"
      if (miss != "") print file ": " nctl " interactive control(s) (first: " first ", line " firstline ") but missing" miss
    }
  }
  FNR == 1 { flush(); file = FILENAME; nctl = 0; hasrole = 0; hasname = 0; first = ""; firstline = 0; inblock = 0; prev_allow = 0 }
  {
    s = $0
    allow = (s ~ /aedst-allow:[ \t]*[^ \t]/)
    if (s ~ /aedst-allow:[ \t]*$/) print FILENAME ":" FNR ": aedst-allow needs a reason"
    if (inblock) { if (match(s, /\*\//)) { s = substr(s, RSTART + 2); inblock = 0 } else s = "" }
    while (match(s, /\/\*/)) {
      rest = substr(s, RSTART + 2)
      if (match(rest, /\*\//)) s = substr(s, 1, RSTART - 1) " " substr(rest, RSTART + 2)
      else { s = substr(s, 1, RSTART - 1); inblock = 1; break }
    }
    sub(/(^|[^:"])\/\/.*/, "", s)
    if (s ~ /Accessible[.]role[ \t]*:/) hasrole = 1
    if (s ~ /Accessible[.]name[ \t]*:/) hasname = 1
    re = "(^|[^A-Za-z0-9_])([A-Za-z0-9_]+[.])?(Button|TextField|TextArea|ItemDelegate|MouseArea|ComboBox|Switch|CheckBox|RadioButton|Slider|SpinBox|ToolButton|TabButton)[ \t]*[{]"
    if (match(s, re) && !(allow || prev_allow)) {
      nctl++
      if (first == "") { first = substr(s, RSTART, RLENGTH); gsub(/^[^A-Za-z0-9_]|[ \t]*[{]$/, "", first); firstline = FNR }
    }
    prev_allow = allow
  }
  END { flush() }' || true)

if [ -n "$out" ]; then
  echo "::error::[qml-accessibility] interactive QML controls without Accessible.role and Accessible.name in the same file:"
  echo "$out" | sed 's/^/  /'
  status=1
fi
[ "$status" -eq 0 ] && echo "OK: QML accessibility check passed ($(echo "$files" | wc -l | tr -d ' ') files)."
exit "$status"
