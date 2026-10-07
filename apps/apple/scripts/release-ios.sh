#!/usr/bin/env bash
# Archive App_iOS and export (or upload) it for App Store Connect / TestFlight.
#
#   APPLE_TEAM_ID=ABCDE12345 scripts/release-ios.sh            # exports build/ios/export/AEDST.ipa
#   UPLOAD=1 APPLE_TEAM_ID=... ASC_KEY_PATH=... ASC_KEY_ID=... ASC_ISSUER_ID=... scripts/release-ios.sh
#
# Requires a Mac with Xcode, Tuist (mise), buf and the protoc plugins on PATH (see README "Getting started").
# NOT executed in the authoring environment (Windows): the first real run is its acceptance test.
set -euo pipefail
cd "$(dirname "$0")/.."
# shellcheck source=scripts/_release-common.sh
source scripts/_release-common.sh

require_env APPLE_TEAM_ID
BUILD_NUMBER="${BUILD_NUMBER:-$(date +%Y%m%d%H%M)}"
OUT=build/ios
rm -rf "$OUT"
mkdir -p "$OUT"

generate_workspace

# shellcheck disable=SC2046
xcodebuild archive \
  -workspace AEDST.xcworkspace \
  -scheme App_iOS \
  -configuration Release \
  -destination "generic/platform=iOS" \
  -archivePath "$OUT/AEDST.xcarchive" \
  DEVELOPMENT_TEAM="$APPLE_TEAM_ID" \
  CURRENT_PROJECT_VERSION="$BUILD_NUMBER" \
  $(asc_xcodebuild_auth_args)

assert_privacy_manifest "$OUT/AEDST.xcarchive/Products/Applications/AEDST.app"

export APPLE_TEAM_ID
export EXPORT_DESTINATION="export"
if [ "${UPLOAD:-0}" = "1" ]; then
  EXPORT_DESTINATION="upload"
fi
envsubst < ExportOptions/iOS-AppStore.plist > "$OUT/ExportOptions.plist"

# shellcheck disable=SC2046
xcodebuild -exportArchive \
  -archivePath "$OUT/AEDST.xcarchive" \
  -exportPath "$OUT/export" \
  -exportOptionsPlist "$OUT/ExportOptions.plist" \
  $(asc_xcodebuild_auth_args)

echo "iOS release build $BUILD_NUMBER done: $OUT (destination: $EXPORT_DESTINATION)"
