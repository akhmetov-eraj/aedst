include_guard(GLOBAL)

# --- CPack (TGZ + DEB + RPM) ------------------------------------------------------
set(CPACK_GENERATOR "TGZ;DEB;RPM")
# Explicit prefix: the .desktop file, metainfo and binary must land in /usr/{share,bin}
# for the desktop environment, and scripts/verify-install-linux.sh relies on it.
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")

set(CPACK_DEBIAN_PACKAGE_NAME "aedst-desktop")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "AEDST <dev@aedst.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "net")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
# libsecret is dlopen()/optional in AedstPlatform (falls back to an encrypted file store
# when no secret-service is running), so it is a Recommends, not a hard Depends.
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "libsecret-1-0, gnome-keyring | kwalletmanager")

set(CPACK_RPM_PACKAGE_NAME "aedst-desktop")
set(CPACK_RPM_PACKAGE_LICENSE "Proprietary")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Internet")
set(CPACK_RPM_PACKAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_RELEASE 1)
# Do not claim ownership of system directories (/usr, /usr/bin, ...): avoids file
# conflicts with the filesystem package on Fedora/openSUSE.
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
    "/usr/share/applications;/usr/share/metainfo;/usr/bin;/usr/lib;/usr/share")
# Soft dependency, see the DEB note above (Suggests is RPM's Recommends analogue on rpm >= 4.12).
set(CPACK_RPM_PACKAGE_SUGGESTS "libsecret")

# Desktop integration files are shared by the DEB, the RPM, the AppImage and Flatpak.
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
