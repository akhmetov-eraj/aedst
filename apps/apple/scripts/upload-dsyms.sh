#!/usr/bin/env bash
# Uploads every dSYM under <path> to Sentry so crash reports are symbolicated.
# Debug files are matched by their UUID (debug id), so no release name is
# needed here; the app reports release "<bundle id>@<version>+<build>" itself
# (Core/AedstTelemetry/TelemetryConfiguration.swift).
#
# Usage: SENTRY_AUTH_TOKEN=... SENTRY_ORG=... SENTRY_PROJECT=... scripts/upload-dsyms.sh <path>
#   <path>: an .xcarchive (Products + dSYMs) or a DerivedData Build/Products directory.
# Called by .github/workflows/apple-ci.yml (main pushes) and meant for
# apple-release.yml right after `xcodebuild archive`.
set -euo pipefail

: "${SENTRY_AUTH_TOKEN:?SENTRY_AUTH_TOKEN is required}"
: "${SENTRY_ORG:?SENTRY_ORG is required}"
: "${SENTRY_PROJECT:?SENTRY_PROJECT is required}"
search_path="${1:?usage: upload-dsyms.sh <path>}"

# sentry-cli 3.8.0 = latest stable, verified 2026-10-07 (GitHub releases API).
SENTRY_CLI_VERSION="${SENTRY_CLI_VERSION:-3.8.0}"
bin="${RUNNER_TEMP:-${TMPDIR:-/tmp}}/sentry-cli-${SENTRY_CLI_VERSION}"
if [ ! -x "$bin" ]; then
  curl -sSfL -o "$bin" \
    "https://github.com/getsentry/sentry-cli/releases/download/${SENTRY_CLI_VERSION}/sentry-cli-Darwin-universal"
  chmod +x "$bin"
fi

"$bin" debug-files upload \
  --include-sources \
  --org "$SENTRY_ORG" \
  --project "$SENTRY_PROJECT" \
  "$search_path"
