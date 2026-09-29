#!/usr/bin/env bash
# Fails if apps/server/api/proto/aedst/** changed without a matching update
# under apps/desktop/Modules/Core/Network (the hand-mirrored DTOs/endpoints).
#
# apps/desktop has no buf/protoc step — see docs/content/architecture-desktop.md
# ("Networking: Connect over Qt Network, no codegen"): DTOs in
# Modules/Core/Network/include/aedst/network/dto/ and the procedure paths in
# endpoints.hpp are hand-mirrored from apps/server/api/proto and "must be
# updated with the protos". There's nothing to regenerate, so this can't
# mirror apps/web's `pnpm connect:generate && git diff --exit-code` literally,
# but the failure mode it exists to prevent is the same: a proto change that
# ships with no corresponding client-side update. See
# apps/server/api/proto/aedst/auth/v1/auth.proto's Register/GetCurrentUser
# additions and the profile/users -> auth merge for a real example of the
# kind of drift this is meant to catch.
set -euo pipefail

cd "$(dirname "$0")/../../.." # repo root

range="${1:?usage: check-proto-freshness.sh <git-diff-range>}"

if ! git rev-list --quiet "$range" >/dev/null 2>&1; then
  echo "::warning::check-proto-freshness.sh: can't resolve diff range '$range' (shallow clone, new branch, or force-push?) — skipping."
  exit 0
fi

proto_changed=$(git diff --name-only "$range" -- apps/server/api/proto/aedst || true)
network_changed=$(git diff --name-only "$range" -- \
  apps/desktop/Modules/Core/Network/include/aedst/network \
  apps/desktop/Modules/Core/Network/src || true)

if [ -n "$proto_changed" ] && [ -z "$network_changed" ]; then
  echo "::error::apps/server/api/proto/aedst changed but apps/desktop/Modules/Core/Network (hand-mirrored DTOs/endpoints) wasn't touched."
  echo
  echo "Changed protos:"
  echo "$proto_changed"
  echo
  echo "Desktop's ApiClient speaks Connect JSON directly without codegen; DTOs" \
       "in Modules/Core/Network/include/aedst/network/dto/ and the procedure" \
       "paths in endpoints.hpp must be hand-updated to match (see" \
       "docs/content/architecture-desktop.md). If no client-visible field or" \
       "endpoint actually changed, note that in the PR description."
  exit 1
fi

echo "OK: no apps/server/api/proto/aedst changes without a matching apps/desktop/Modules/Core/Network update."
