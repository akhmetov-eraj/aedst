#!/usr/bin/env bash
# Stop hook for apps/server: before the agent hands control back, run the
# same fast gate CI runs (task check) and block the stop if it fails,
# feeding the failure back to the model instead of leaving a red build for
# a human to discover later.
set -euo pipefail

INPUT="$(cat)"
STOP_HOOK_ACTIVE="$(echo "$INPUT" | jq -r '.stop_hook_active // false' 2>/dev/null || echo false)"
[ "$STOP_HOOK_ACTIVE" = "true" ] && exit 0

ROOT="${CLAUDE_PROJECT_DIR:-.}/apps/server"
[ -d "$ROOT" ] || ROOT="."
cd "$ROOT" || exit 0

command -v task >/dev/null 2>&1 || exit 0

OUTPUT="$(task check 2>&1)" && exit 0

if command -v jq >/dev/null 2>&1; then
  jq -n --arg reason "task check failed:\n$OUTPUT" \
    '{hookSpecificOutput: {hookEventName: "Stop", permissionDecision: "deny", permissionDecisionReason: $reason}}'
  exit 0
fi

echo "task check failed:" >&2
echo "$OUTPUT" >&2
exit 2
