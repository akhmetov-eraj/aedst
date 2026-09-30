include_guard(GLOBAL)

# --- CPack: DMG (DragNDrop) + PKG (productbuild) --------------------------------
# Both wrap aedst-desktop.app, which install(TARGETS ... BUNDLE DESTINATION .) and the
# Qt deploy script (macdeployqt via qt_generate_deploy_qml_app_script) assemble.
#
# Signing and notarization are NOT done by CMake configuration: they need secrets and
# run in CI (or locally) through packaging/macos/sign-and-notarize.sh. The one
# exception is the bundle itself: MacOSPreBuild.cmake re-signs the staged .app
# (hardened runtime + entitlements) inside CPack when APPLE_SIGNING_IDENTITY is set, so
# the DMG/PKG contain a signed app. Without the env var everything builds unsigned
# (fine for CI on forks / local dev; macOS Gatekeeper will refuse to open it).
set(CPACK_GENERATOR "DragNDrop;productbuild")

# Arch in the file name: arm64 and x86_64 builds are separate trees (see macos-* presets).
if(CMAKE_OSX_ARCHITECTURES)
    set(aedst_mac_arch "${CMAKE_OSX_ARCHITECTURES}")
else()
    set(aedst_mac_arch "${CMAKE_SYSTEM_PROCESSOR}")
endif()
string(REPLACE ";" "-" aedst_mac_arch "${aedst_mac_arch}")
set(CPACK_PACKAGE_FILE_NAME "aedst-desktop-${PROJECT_VERSION}-macOS-${aedst_mac_arch}")

# DMG
set(CPACK_DMG_FORMAT "UDZO")
set(CPACK_DMG_VOLUME_NAME "${AEDST_DISPLAY_NAME} ${PROJECT_VERSION}")
# CPACK_DMG_BACKGROUND_IMAGE / CPACK_DMG_DS_STORE: cosmetic, add with real artwork.

# PKG (productbuild). The install prefix (/Applications) is per-generator and set
# in MacOSCPackConfig.cmake; identity names come from the environment there too.
set(CPACK_PRODUCTBUILD_IDENTIFIER "${AEDST_APP_ID}")
set(CPACK_PRODUCTBUILD_DOMAIN "system")
set(CPACK_PKGBUILD_IDENTIFIER "${AEDST_APP_ID}")

set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_CURRENT_LIST_DIR}/MacOSCPackConfig.cmake")
set(CPACK_PRE_BUILD_SCRIPTS "${CMAKE_CURRENT_LIST_DIR}/MacOSPreBuild.cmake")

# Kept next to the package metadata so cpack-time scripts can find the helper/entitlements
# (CPACK_* variables are exported into CPackConfig.cmake, plain ones are not).
set(CPACK_AEDST_SIGN_SCRIPT "${AEDST_PACKAGING_DIR}/macos/sign-and-notarize.sh")
set(CPACK_AEDST_ENTITLEMENTS "${AEDST_PACKAGING_DIR}/macos/entitlements.plist")
