include_guard(GLOBAL)

# --- CPack (TGZ + DEB) ------------------------------------------------------------
set(CPACK_GENERATOR "TGZ;DEB")
set(CPACK_DEBIAN_PACKAGE_NAME "aedst-desktop")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "AEDST <dev@aedst.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "net")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

# Desktop integration files are shared by the DEB, the AppImage and Flatpak.
install(FILES "${AEDST_PACKAGING_DIR}/linux/flatpak/${AEDST_APP_ID}.desktop"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
install(FILES "${AEDST_PACKAGING_DIR}/linux/flatpak/${AEDST_APP_ID}.metainfo.xml"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo")

# --- AppImage (package_appimage) -------------------------------------------------
# Needs linuxdeploy + linuxdeploy-plugin-qt on PATH (or $LINUXDEPLOY) and a real
# 256x256 icon; see packaging/linux/appimage/build-appimage.sh.
add_custom_target(package_appimage
    COMMAND "${CMAKE_COMMAND}" -E env
            "AEDST_BUILD_DIR=${CMAKE_BINARY_DIR}"
            "AEDST_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            "AEDST_VERSION=${PROJECT_VERSION}"
            bash "${AEDST_PACKAGING_DIR}/linux/appimage/build-appimage.sh"
    DEPENDS aedst-desktop
    USES_TERMINAL
    COMMENT "Building AppImage via linuxdeploy"
    VERBATIM)

# --- Flatpak (package_flatpak) ---------------------------------------------------
# Builds from the manifest in a sandbox against org.kde.Platform (which provides
# Qt), independent of this CMake build tree. Scope cut: publishing to a Flatpak
# repo / Flathub is not wired anywhere yet.
find_program(AEDST_FLATPAK_BUILDER flatpak-builder)
if(AEDST_FLATPAK_BUILDER)
    add_custom_target(package_flatpak
        COMMAND "${AEDST_FLATPAK_BUILDER}" --force-clean --user --install-deps-from=flathub
                "--repo=${CMAKE_BINARY_DIR}/packaging/flatpak/repo"
                "${CMAKE_BINARY_DIR}/packaging/flatpak/build-dir"
                "${AEDST_PACKAGING_DIR}/linux/flatpak/${AEDST_APP_ID}.yml"
        COMMAND flatpak build-bundle "${CMAKE_BINARY_DIR}/packaging/flatpak/repo"
                "${CMAKE_BINARY_DIR}/packages/aedst-desktop-${PROJECT_VERSION}.flatpak" "${AEDST_APP_ID}"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        USES_TERMINAL
        COMMENT "Building Flatpak bundle from ${AEDST_APP_ID}.yml"
        VERBATIM)
else()
    message(STATUS "AEDST: flatpak-builder not found; package_flatpak target disabled")
endif()
