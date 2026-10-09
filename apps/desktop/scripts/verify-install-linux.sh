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

# The bundled Qt must live in its private directory only: a libQt6* (or plugins/qml) directly under /usr/lib would shadow or
# conflict with the distribution's Qt packages (cmake/AedstLinuxBundle.cmake).
stray=$(printf '%s\n' "${owned[@]}" | grep -E '^/usr/(lib|lib64|lib/[a-z0-9_-]+-linux-gnu)/(lib(Qt6|icu)[^/]*\.so|plugins/|qml/)' || true)
if [ -n "$stray" ]; then
  echo "::error::package installs Qt-shaped files outside /usr/lib*/aedst-desktop:"; echo "$stray"; exit 1
fi
grep -Eq '/aedst-desktop/libQt6Core\.so' <<<"$(printf '%s\n' "${owned[@]}")" || {
  echo "::error::the bundled libQt6Core.so is not under .../aedst-desktop/ (was the package built with AEDST_DEPLOY_QT=ON?)"; exit 1; }

# Exploit mitigations of the shipped binary (cmake/AedstHardening.cmake). readelf is always there (binutils); checksec,
# when installed, gives a second opinion in the log. A missing mitigation fails the job.
command -v readelf >/dev/null || { echo "::error::readelf not found (apt install binutils)"; exit 1; }
hardening_fail=0
need() { echo "hardening: $1"; }
elf_type=$(readelf -h "$bin" | awk '/Type:/ {print $2}')
[ "$elf_type" = "DYN" ] || { echo "::error::not a PIE executable (ELF type $elf_type)"; hardening_fail=1; }
readelf -lW "$bin" | grep -q 'GNU_RELRO' || { echo "::error::no RELRO segment"; hardening_fail=1; }
readelf -dW "$bin" | grep -Eq 'BIND_NOW|FLAGS.*NOW' || { echo "::error::no BIND_NOW (full RELRO)"; hardening_fail=1; }
readelf -lW "$bin" | awk '/GNU_STACK/ {print $NF" "$(NF-1)}' | grep -q 'RWE' && { echo "::error::executable stack"; hardening_fail=1; }
readelf -sW "$bin" | grep -q '__stack_chk_fail' || { echo "::error::no stack protector (__stack_chk_fail not referenced)"; hardening_fail=1; }
# FORTIFY_SOURCE leaves *_chk imports behind (memcpy_chk, ...); a Release binary of this size always calls some.
readelf -sW "$bin" | grep -Eq '_chk(@|$| )' || { echo "::warning::no *_chk (FORTIFY_SOURCE) imports found - check _FORTIFY_SOURCE=3 reached the Release build"; }
runpath=$(readelf -dW "$bin" | awk '/RUNPATH|RPATH/ {print $NF}')
grep -q 'aedst-desktop' <<<"$runpath" || { echo "::error::RUNPATH does not point into the private Qt directory (got: $runpath)"; hardening_fail=1; }
need "PIE=$elf_type RUNPATH=$runpath"
if command -v checksec >/dev/null; then checksec --file="$bin" || true; fi
[ "$hardening_fail" -eq 0 ] || exit 1

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
