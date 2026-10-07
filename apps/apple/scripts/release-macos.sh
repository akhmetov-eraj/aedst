#!/usr/bin/env bash
# Archive App_macOS and produce either a notarized Developer ID build (default)
# or a Mac App Store build.
#
#   APPLE_TEAM_ID=ABCDE12345 ASC_KEY_PATH=... ASC_KEY_ID=... ASC_ISSUER_ID=... scripts/release-macos.sh
#   DISTRIBUTION=app-store UPLOAD=1 ... scripts/release-macos.sh
#
# Developer ID needs a "Developer ID Application" certificate in the keychain; notarization
# needs an App Store Connect API key (or NOTARY_KEYCHAIN_PROFILE created with `notarytool store-credentials`).
# NOT executed in the authoring environment (Windows): the first real run is its acceptance test.
set -euo pipefail
cd "$(dirname "$0")/.."
# shellcheck source=scripts/_release-common.sh
source scripts/_release-common.sh

require_env APPLE_TEAM_ID
DISTRIBUTION="${DISTRIBUTION:-developer-id}"
BUILD_NUMBER="${BUILD_NUMBER:-$(date +%Y%m%d%H%M)}"
OUT=build/macos
rm -rf "$OUT"
mkdir -p "$OUT"

generate_workspace

# shellcheck disable=SC2046
xcodebuild archive \
  -workspace AEDST.xcworkspace \
  -scheme App_macOS \
  -configuration Release \
  -destination "generic/platform=macOS" \
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

case "$DISTRIBUTION" in
  developer-id) OPTIONS=ExportOptions/macOS-DeveloperID.plist ;;
  app-store)    OPTIONS=ExportOptions/macOS-AppStore.plist ;;
  *) echo "error: DISTRIBUTION must be developer-id or app-store" >&2; exit 2 ;;
esac
envsubst < "$OPTIONS" > "$OUT/ExportOptions.plist"

# shellcheck disable=SC2046
xcodebuild -exportArchive \
  -archivePath "$OUT/AEDST.xcarchive" \
  -exportPath "$OUT/export" \
  -exportOptionsPlist "$OUT/ExportOptions.plist" \
  $(asc_xcodebuild_auth_args)

if [ "$DISTRIBUTION" = "developer-id" ]; then
  APP="$OUT/export/AEDST.app"

  # Hardened runtime is mandatory for notarization.
  codesign --verify --deep --strict --verbose=2 "$APP"
  if ! codesign -dv "$APP" 2>&1 | grep -q "flags=.*runtime"; then
    echo "error: hardened runtime is not enabled on $APP" >&2
    exit 1
  fi

  ditto -c -k --keepParent "$APP" "$OUT/AEDST-notarize.zip"

  if [ -n "${NOTARY_KEYCHAIN_PROFILE:-}" ]; then
    NOTARY_AUTH=(--keychain-profile "$NOTARY_KEYCHAIN_PROFILE")
  else
    require_env ASC_KEY_PATH ASC_KEY_ID ASC_ISSUER_ID
    NOTARY_AUTH=(--key "$ASC_KEY_PATH" --key-id "$ASC_KEY_ID" --issuer "$ASC_ISSUER_ID")
  fi
  xcrun notarytool submit "$OUT/AEDST-notarize.zip" "${NOTARY_AUTH[@]}" --wait

  xcrun stapler staple "$APP"
  xcrun stapler validate "$APP"
  spctl --assess --type execute --verbose=4 "$APP"

  ditto -c -k --keepParent "$APP" "$OUT/AEDST-$BUILD_NUMBER.zip"
  echo "Notarized, stapled build: $OUT/AEDST-$BUILD_NUMBER.zip"
else
  echo "Mac App Store build $BUILD_NUMBER done: $OUT (destination: $EXPORT_DESTINATION)"
fi
