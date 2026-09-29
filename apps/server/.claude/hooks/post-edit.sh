#!/usr/bin/env bash
# PostToolUse hook (Edit|Write) for apps/server: keeps generated/formatted
# code from drifting while an agent is actively editing, instead of only
# catching it at `task check` time.
set -euo pipefail

ROOT="${CLAUDE_PROJECT_DIR:-.}/apps/server"
[ -d "$ROOT" ] || ROOT="."
cd "$ROOT" || exit 0

FILE_PATH="$(jq -r '.tool_input.file_path // empty' 2>/dev/null || true)"
[ -n "$FILE_PATH" ] || exit 0

case "$FILE_PATH" in
  *.go)
    command -v gofumpt >/dev/null 2>&1 && gofumpt -w "$FILE_PATH" || true
    ;;
  */queries/*.sql|*/sql/*/*.sql|*.proto)
    command -v task >/dev/null 2>&1 && task gen >/dev/null 2>&1 || true
    ;;
esac

exit 0
