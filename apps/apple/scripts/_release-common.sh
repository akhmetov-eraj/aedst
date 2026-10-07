#!/usr/bin/env bash
# Shared helpers for release-ios.sh / release-macos.sh. Sourced, not executed.
set -euo pipefail

require_env() {
  for name in "$@"; do
    if [ -z "${!name:-}" ]; then
      echo "error: environment variable $name is required" >&2
      exit 2
    fi
  done
}

# App Store Connect API key auth (used by xcodebuild -allowProvisioningUpdates and notarytool).
# ASC_KEY_PATH (a .p8 file), ASC_KEY_ID and ASC_ISSUER_ID are optional locally (the Xcode
# signed-in account is used instead) and provided by CI.
asc_xcodebuild_auth_args() {
  if [ -n "${ASC_KEY_PATH:-}" ] && [ -n "${ASC_KEY_ID:-}" ] && [ -n "${ASC_ISSUER_ID:-}" ]; then
    echo "-allowProvisioningUpdates -authenticationKeyPath ${ASC_KEY_PATH} -authenticationKeyID ${ASC_KEY_ID} -authenticationKeyIssuerID ${ASC_ISSUER_ID}"
  else
    echo "-allowProvisioningUpdates"
  fi
}

generate_workspace() {
  # Connect stubs are a build input (generated from apps/server/api/proto, not committed).
  make generate
  tuist install
  tuist generate --no-open
}

# The App Store rejects binaries without a privacy manifest; fail early and locally.
assert_privacy_manifest() {
  local app="$1"
  if ! find "$app" -name 'PrivacyInfo.xcprivacy' -print -quit | grep -q .; then
    echo "error: no PrivacyInfo.xcprivacy inside $app" >&2
    exit 1
  fi
}
