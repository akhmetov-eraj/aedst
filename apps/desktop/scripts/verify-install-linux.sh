#!/usr/bin/env bash
# Install-verify for the .deb on a CLEAN runner:
#   install -> `aedst-desktop --version` prints the expected version -> purge ->
#   every file the package owned is gone.
#
#   ./scripts/verify-install-linux.sh <path/to/aedst-desktop-X.Y.Z-*.deb> <expected-version>
#
# Requires main.cpp to handle `--version` (print AEDST_VERSION, exit 0, no display).
set -euo pipefail

deb="${1:?usage: verify-install-linux.sh <deb> <version>}"
version="${2:?usage: verify-install-linux.sh <deb> <version>}"
pkg=aedst-desktop

sudo apt-get update
# apt (not bare dpkg -i) so the package's Depends are resolved on the clean image.
sudo apt-get install -y "./${deb}"

mapfile -t owned < <(dpkg -L "$pkg" | while read -r p; do [ -f "$p" ] && echo "$p"; done)
echo "package owns ${#owned[@]} files"
[ "${#owned[@]}" -gt 0 ] || { echo "::error::package $pkg installed no files"; exit 1; }

bin=$(dpkg -L "$pkg" | grep -E '/bin/aedst-desktop$' | head -1 || true)
[ -n "$bin" ] && [ -x "$bin" ] || { echo "::error::aedst-desktop binary not installed/executable"; exit 1; }

out=$(QT_QPA_PLATFORM=offscreen timeout 30 "$bin" --version)
echo "--version output: $out"
grep -Fq "$version" <<<"$out" || { echo "::error::--version output does not contain $version"; exit 1; }

sudo apt-get purge -y "$pkg"
if dpkg -s "$pkg" >/dev/null 2>&1; then echo "::error::$pkg still installed after purge"; exit 1; fi
left=0
for f in "${owned[@]}"; do
  if [ -e "$f" ]; then echo "::error::left behind after purge: $f"; left=1; fi
done
[ "$left" -eq 0 ] || exit 1
echo "OK: install / --version / purge verified for $deb"
