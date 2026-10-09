#!/usr/bin/env bash
# Stop hook (repo root): before the agent hands control back, run the cheap cross-platform gate
# (`task check`) and the lint/check task of every app this session edited, and block the stop
# when one fails so the model fixes it instead of leaving a red build.
#
# Stop-hook contract (Claude Code): block with top-level {"decision":"block","reason":"..."} on
# stdout and exit 0 (or reason on stderr + exit 2). `stop_hook_active` is true when the model is
# already continuing because of a Stop hook; honour it or the hook loops forever.
#
# "Touched apps" come from the PostToolUse dispatcher (post-edit.sh), which appends the app name to
# ${TMPDIR:-/tmp}/aedst-claude-touched-<session_id>. A session that edited nothing under apps/ is not gated.
#
# Environment:
#   AEDST_STOP_GATE=off          skip the gate (e.g. while several agents share one working tree)
#   AEDST_STOP_GATE_CMD=<cmd>    replaces the whole check list with one command (used by scripts/check-hooks.sh)
set -uo pipefail

HOOK_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IO="$HOOK_DIR/lib/hook-io.mjs"
ROOT="$(cd "$HOOK_DIR/../.." && pwd)"

[ "${AEDST_STOP_GATE:-on}" = "off" ] && exit 0
command -v node >/dev/null 2>&1 || exit 0

INPUT="$(cat)"
[ "$(printf '%s' "$INPUT" | node "$IO" field stop_active 2>/dev/null)" = "true" ] && exit 0

sid="$(printf '%s' "$INPUT" | node "$IO" field session_id 2>/dev/null || echo unknown)"
state="${TMPDIR:-/tmp}/aedst-claude-touched-$sid"
[ -s "$state" ] || exit 0

cd "$ROOT" || exit 0

cmds=()
if [ -n "${AEDST_STOP_GATE_CMD:-}" ]; then
  cmds+=("$AEDST_STOP_GATE_CMD")
else
  command -v task >/dev/null 2>&1 || exit 0
  cmds+=("task check")
  while read -r app; do
    case "$app" in
      server) cmds+=("task server:check") ;;
      web) cmds+=("task web:lint") ;;
      android) command -v java >/dev/null 2>&1 && cmds+=("task android:lint") ;;
      apple) [ "$(uname -s)" = "Darwin" ] && cmds+=("task apple:lint") ;;
      desktop) cmds+=("task desktop:lint") ;;
    esac
  done < <(sort -u "$state")
fi

failed=""
for c in "${cmds[@]}"; do
  out="$(bash -c "$c" 2>&1)" && continue
  failed="$(printf '`%s` failed. Fix it before stopping.\n\n%s' "$c" "$(printf '%s\n' "$out" | tail -n 60)")"
  break
done
if [ -z "$failed" ]; then
  rm -f "$state" # green: the next turn is only gated if it edits something again
  exit 0
fi

printf '%s' "$failed" | node "$IO" block
exit 0
