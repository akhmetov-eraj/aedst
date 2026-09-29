#!/usr/bin/env bash
# Builds an AppImage from a configured (Release) CMake build tree.
#
# Invoked by the `package_appimage` CMake target, which sets AEDST_BUILD_DIR,
# AEDST_SOURCE_DIR and AEDST_VERSION. Requires on PATH (or via env):
#   - linuxdeploy            ($LINUXDEPLOY,    https://github.com/linuxdeploy/linuxdeploy)
#   - linuxdeploy-plugin-qt  (found by linuxdeploy next to itself or on PATH)
#   - qmake from the same Qt 6 install used for the build ($QMAKE)
#
# Not exercised in CI yet — written against linuxdeploy's documented flags.
set -euo pipefail

: "${AEDST_BUILD_DIR:?set AEDST_BUILD_DIR to the CMake build directory}"
AEDST_SOURCE_DIR="${AEDST_SOURCE_DIR:-$(cd "$(dirname "$0")/../../.." && pwd)}"
AEDST_VERSION="${AEDST_VERSION:-0.0.0}"
LINUXDEPLOY="${LINUXDEPLOY:-linuxdeploy}"

HERE="$(cd "$(dirname "$0")" && pwd)"
WORK_DIR="${AEDST_BUILD_DIR}/packaging/appimage"
APPDIR="${WORK_DIR}/AppDir"
ICON="${AEDST_SOURCE_DIR}/App/resources/icons/com.aedst.Desktop.png"

if [[ ! -f "${ICON}" ]]; then
    echo "error: ${ICON} is missing. linuxdeploy requires a real 256x256 PNG icon;" >&2
    echo "       see App/resources/icons/README.md." >&2
    exit 1
fi

rm -rf "${APPDIR}"
mkdir -p "${WORK_DIR}"

# Configure the build tree with -DAEDST_DEPLOY_QT=OFF for AppImage builds:
# linuxdeploy-plugin-qt does the Qt bundling here and understands AppDir layout,
# so Qt's own deploy script would only duplicate libraries.
cmake --install "${AEDST_BUILD_DIR}" --prefix "${APPDIR}/usr"

export QML_SOURCES_PATHS="${AEDST_SOURCE_DIR}/App/qml:${AEDST_SOURCE_DIR}/Modules"
export VERSION="${AEDST_VERSION}"
export OUTPUT="${AEDST_BUILD_DIR}/packages/aedst-desktop-${AEDST_VERSION}-x86_64.AppImage"
mkdir -p "$(dirname "${OUTPUT}")"

"${LINUXDEPLOY}" \
    --appdir "${APPDIR}" \
    --executable "${APPDIR}/usr/bin/aedst-desktop" \
    --desktop-file "${HERE}/com.aedst.Desktop.desktop" \
    --icon-file "${ICON}" \
    --custom-apprun "${HERE}/AppRun" \
    --plugin qt \
    --output appimage

echo "AppImage written to ${OUTPUT}"
