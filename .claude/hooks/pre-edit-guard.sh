#!/usr/bin/env bash
# PreToolUse hook (matcher Edit|Write|MultiEdit|NotebookEdit): refuse to edit generated files.
# Generated code is regenerated from its source (task gen, pnpm connect:generate, make generate,
# pnpm --dir packages/design-tokens build), never edited by hand. The globs mirror the
# "generated code" block of scripts/file-size.config; keep the two in step.
#
# Contract (Claude Code hooks): reads the hook JSON on stdin; to deny, prints
# {"hookSpecificOutput":{"hookEventName":"PreToolUse","permissionDecision":"deny",...}} and exits 0.
# Anything else (including unparsable input such as `{}`) allows the edit: print nothing, exit 0.
set -uo pipefail

HOOK_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IO="$HOOK_DIR/lib/hook-io.mjs"

command -v node >/dev/null 2>&1 || exit 0
rel="$(node "$IO" field file_path 2>/dev/null || true)"
[ -n "$rel" ] || exit 0

regen=""
case "$rel" in
  apps/server/gen/* | apps/server/internal/*/sqlc/* | apps/server/internal/*/db/*.sql.go | *.pb.go | *_connect.go)
    regen="task server:gen" ;;
  apps/web/src/shared/api/*/generated/* | apps/web/src/gen/* | *_pb.ts | *_pb.js)
    regen="task web:generate (pnpm connect:generate)" ;;
  apps/web/next-env.d.ts)
    regen="next dev / next build rewrites it" ;;
  apps/web/src/shared/ui/tokens.css | packages/design-tokens/dist/* | */AedstTokens.qml | */AedstLightPalette.* | */AedstDarkPalette.*)
    regen="pnpm --dir packages/design-tokens build" ;;
  apps/android/core/network/src/main/kotlin-gen/*)
    regen="task android:generate (make generate)" ;;
  apps/apple/Core/*/Generated/* | apps/apple/*/Derived/*)
    regen="task apple:generate (make generate) / tuist generate" ;;
  apps/desktop/*/generated/* | apps/desktop/docs/generated/*)
    regen="the desktop CMake build (generated at build time)" ;;
  *.pb.* | *.connect.*)
    regen="the buf/protoc generation step of the owning app" ;;
esac

[ -n "$regen" ] || exit 0

node "$IO" deny "Refusing to edit generated file '$rel'. Change its source and regenerate: $regen. See AGENTS.md (\"Generated files are never hand-edited\")."
exit 0
