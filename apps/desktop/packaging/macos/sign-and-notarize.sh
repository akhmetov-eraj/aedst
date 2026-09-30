#!/usr/bin/env bash
# Code signing + notarization helper for the macOS artifacts. All inputs come from the
# environment (never from arguments, never printed):
#
#   APPLE_SIGNING_IDENTITY       "Developer ID Application: Name (TEAMID)"   sign-app, sign-file
#   APPLE_INSTALLER_IDENTITY     "Developer ID Installer: Name (TEAMID)"     sign-pkg
#   Notarization, ONE of:
#     APPLE_ID + APPLE_TEAM_ID + APPLE_APP_SPECIFIC_PASSWORD               (Apple ID)
#     APPLE_API_KEY_PATH + APPLE_API_KEY_ID + APPLE_API_ISSUER             (App Store Connect API key)
#     APPLE_NOTARY_PROFILE                                                 (stored `notarytool store-credentials`)
#   AEDST_ENTITLEMENTS           entitlements plist (default: entitlements.plist next to this script)
#
# Subcommands
#   sign-app <path/to/aedst-desktop.app>   sign nested code inside-out, then the bundle
#                                          (hardened runtime + timestamp + entitlements)
#   sign-file <file.dmg|file.zip>          sign a disk image
#   sign-pkg <in.pkg> <out.pkg>            productsign an installer package
#   notarize <file.dmg|file.pkg|file.zip>  xcrun notarytool submit --wait, then xcrun stapler staple
#   verify <path>                          codesign --verify (+ spctl assessment for .app/.pkg)
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
ent="${AEDST_ENTITLEMENTS:-$here/entitlements.plist}"

need_env() { [ -n "${!1:-}" ] || { echo "error: \$$1 is not set" >&2; exit 2; }; }

sign_one() { # sign_one <path> [extra codesign args...]
  local target="$1"; shift
  codesign --force --timestamp --options runtime -s "$APPLE_SIGNING_IDENTITY" "$@" "$target"
}

cmd="${1:-}"; shift || true
case "$cmd" in
  sign-app)
    app="${1:?usage: sign-app <app>}"
    need_env APPLE_SIGNING_IDENTITY
    [ -d "$app" ] || { echo "error: $app is not a bundle" >&2; exit 2; }
    # Inside-out: loose libraries/plugins, then frameworks, then the main bundle.
    # (--deep is deliberately avoided: it is deprecated for signing and hides ordering bugs.)
    find "$app/Contents" -type f \( -name '*.dylib' -o -name '*.so' \) -print0 \
      | while IFS= read -r -d '' f; do sign_one "$f"; done
    if [ -d "$app/Contents/Frameworks" ]; then
      find "$app/Contents/Frameworks" -maxdepth 1 -name '*.framework' -print0 \
        | while IFS= read -r -d '' f; do sign_one "$f"; done
    fi
    sign_one "$app" --entitlements "$ent"
    codesign --verify --deep --strict --verbose=2 "$app"
    ;;
  sign-file)
    f="${1:?usage: sign-file <file>}"
    need_env APPLE_SIGNING_IDENTITY
    codesign --force --timestamp -s "$APPLE_SIGNING_IDENTITY" "$f"
    codesign --verify --verbose=2 "$f"
    ;;
  sign-pkg)
    in="${1:?usage: sign-pkg <in.pkg> <out.pkg>}"; out="${2:?usage: sign-pkg <in.pkg> <out.pkg>}"
    need_env APPLE_INSTALLER_IDENTITY
    productsign --sign "$APPLE_INSTALLER_IDENTITY" --timestamp "$in" "$out"
    pkgutil --check-signature "$out"
    ;;
  notarize)
    f="${1:?usage: notarize <file>}"
    if [ -n "${APPLE_NOTARY_PROFILE:-}" ]; then
      auth=(--keychain-profile "$APPLE_NOTARY_PROFILE")
    elif [ -n "${APPLE_API_KEY_PATH:-}" ]; then
      need_env APPLE_API_KEY_ID; need_env APPLE_API_ISSUER
      auth=(--key "$APPLE_API_KEY_PATH" --key-id "$APPLE_API_KEY_ID" --issuer "$APPLE_API_ISSUER")
    else
      need_env APPLE_ID; need_env APPLE_TEAM_ID; need_env APPLE_APP_SPECIFIC_PASSWORD
      auth=(--apple-id "$APPLE_ID" --team-id "$APPLE_TEAM_ID" --password "$APPLE_APP_SPECIFIC_PASSWORD")
    fi
    xcrun notarytool submit "$f" "${auth[@]}" --wait
    xcrun stapler staple "$f"
    xcrun stapler validate "$f"
    ;;
  verify)
    p="${1:?usage: verify <path>}"
    case "$p" in
      *.pkg) pkgutil --check-signature "$p"; spctl --assess --type install --verbose=2 "$p" ;;
      *.app) codesign --verify --deep --strict --verbose=2 "$p"; spctl --assess --type execute --verbose=2 "$p" ;;
      *)     codesign --verify --verbose=2 "$p" ;;
    esac
    ;;
  *)
    sed -n '2,25p' "$0" >&2
    exit 2
    ;;
esac
