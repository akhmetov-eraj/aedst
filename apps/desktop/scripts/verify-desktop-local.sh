#!/usr/bin/env bash
# One-shot local verification: configure + build + test with a preset.
#
#   ./scripts/verify-desktop-local.sh [preset]      # default: host-appropriate debug preset
#   ./scripts/verify-desktop-local.sh gcc-asan
#   ./scripts/verify-desktop-local.sh --lint         # only the fast static checks
#
# Presets: msvc-debug gcc-debug clang-debug macos-debug (+ -asan/-tsan variants,
# coverage). See CMakePresets.json. Checks that the required tools exist first and
# says how to get each missing one, instead of failing 30 seconds into cmake.
set -euo pipefail

cd "$(dirname "$0")/.."

missing=0
need() { # need <cmd> <hint>
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "missing: $1  ($2)" >&2
    missing=1
  fi
}

if [ "${1:-}" = "--lint" ]; then
  bash ./scripts/check-naming.sh
  bash ./scripts/check-format.sh
  bash ./scripts/check-feature-boundaries.sh
  bash ./scripts/check-proto-freshness.sh
  bash ./scripts/check-qml-lint.sh
  echo "lint OK"
  exit 0
fi

preset="${1:-}"
if [ -z "$preset" ]; then
  case "$(uname -s)" in
    Linux)   preset=gcc-debug ;;
    Darwin)  preset=macos-debug ;;
    MINGW*|MSYS*|CYGWIN*) preset=msvc-debug ;;
    *) echo "unknown host; pass a preset explicitly" >&2; exit 2 ;;
  esac
fi

need cmake  "https://cmake.org/download, >= 3.25 (CI uses 3.30.5)"
need ctest  "ships with cmake"
need ninja  "apt install ninja-build | brew install ninja | VS installs one"

case "$preset" in
  gcc-*|coverage)  need g++ "apt install g++-13 or newer" ;;
  clang-*)         need clang++ "apt install clang-17 or newer" ;;
  msvc-*)          need cl "run from a VS 2022 Developer prompt (or vcvars64.bat) so Ninja finds cl.exe" ;;
  macos-*)         need clang++ "xcode-select --install" ;;
esac
case "$preset" in
  coverage) need gcovr "pip install gcovr" ;;
esac

if [ -z "${VCPKG_ROOT:-}" ]; then
  echo "missing: VCPKG_ROOT is not set (clone https://github.com/microsoft/vcpkg and export VCPKG_ROOT)" >&2
  missing=1
elif [ ! -f "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" ]; then
  echo "missing: \$VCPKG_ROOT ($VCPKG_ROOT) does not contain scripts/buildsystems/vcpkg.cmake" >&2
  missing=1
fi
if [ -z "${QT_ROOT_DIR:-}" ]; then
  echo "missing: QT_ROOT_DIR is not set (Qt $(tr -d '[:space:]' < qt-version.txt) from the official installer/aqtinstall, e.g. .../6.12.0/gcc_64)" >&2
  missing=1
fi
[ "$(uname -s)" = Linux ] && ! pkg-config --exists libsecret-1 2>/dev/null \
  && echo "note: libsecret-1-dev not found; AedstPlatform falls back per its own CMake (apt install libsecret-1-dev)"

if [ "$missing" -ne 0 ]; then
  echo "verify-desktop-local: fix the items above and re-run." >&2
  exit 127
fi

export QT_QPA_PLATFORM=offscreen
echo "==> configure ($preset)"
cmake --preset "$preset"
echo "==> build ($preset)"
cmake --build --preset "$preset"
if [ "$preset" = coverage ]; then
  echo "==> coverage report"
  cmake --build --preset coverage-report
else
  echo "==> test ($preset)"
  ctest --preset "$preset"
fi
echo "verify-desktop-local: $preset OK"
