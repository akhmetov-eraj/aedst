#!/usr/bin/env bash
# Install-verify for the macOS artifacts on a CLEAN runner.
#
#   ./scripts/verify-install-macos.sh <file.dmg|file.pkg> <expected-version>
#
#   .dmg: mount, copy the .app out, unmount, run `Contents/MacOS/aedst-desktop --version`.
#   .pkg: `installer -target /`, run the installed app, then delete it (a .pkg has no
#         uninstaller; removing the bundle is the documented removal) and verify it is gone.
# AEDST_EXPECT_SIGNED=1 additionally requires a valid code signature (+ notarization
# ticket check with spctl for the .app).
# Requires main.cpp to handle `--version` (print AEDST_VERSION, exit 0, no display).
set -euo pipefail

artifact="${1:?usage: verify-install-macos.sh <dmg|pkg> <version>}"
version="${2:?usage: verify-install-macos.sh <dmg|pkg> <version>}"
app_name="aedst-desktop.app"

check_app() { # check_app <path-to-.app>
  local app="$1" out
  [ -d "$app" ] || { echo "::error::$app not found"; exit 1; }
  if [ "${AEDST_EXPECT_SIGNED:-0}" = "1" ]; then
    codesign --verify --deep --strict --verbose=2 "$app"
    spctl --assess --type execute --verbose=4 "$app" || {
      echo "::error::spctl rejected $app (unsigned or not notarized)"; exit 1; }
  fi
  out=$(QT_QPA_PLATFORM=offscreen "$app/Contents/MacOS/aedst-desktop" --version)
  echo "--version output: $out"
  grep -Fq "$version" <<<"$out" || { echo "::error::--version output does not contain $version"; exit 1; }
}

case "$artifact" in
  *.dmg)
    mnt=$(mktemp -d)
    hdiutil attach "$artifact" -nobrowse -readonly -mountpoint "$mnt"
    trap 'hdiutil detach "$mnt" -force >/dev/null 2>&1 || true' EXIT
    dest=$(mktemp -d)
    cp -R "$mnt/$app_name" "$dest/"
    hdiutil detach "$mnt"
    trap - EXIT
    check_app "$dest/$app_name"
    rm -rf "$dest"
    ;;
  *.pkg)
    sudo installer -pkg "$artifact" -target /
    check_app "/Applications/$app_name"
    sudo rm -rf "/Applications/$app_name"
    [ ! -e "/Applications/$app_name" ] || { echo "::error::app still present after removal"; exit 1; }
    ;;
  *) echo "unsupported artifact: $artifact" >&2; exit 2 ;;
esac
echo "OK: $artifact verified"
