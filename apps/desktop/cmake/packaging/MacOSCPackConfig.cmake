# Loaded by cpack (CPACK_PROJECT_CONFIG_FILE) once per generator, so settings that
# differ between DragNDrop and productbuild live here, not in macOS.cmake.

if(CPACK_GENERATOR STREQUAL "productbuild")
    # The .pkg installs the app into /Applications.
    set(CPACK_PACKAGING_INSTALL_PREFIX "/Applications")

    # Installer signing (Developer ID Installer certificate in the keychain). Optional.
    if(NOT "$ENV{APPLE_INSTALLER_IDENTITY}" STREQUAL "")
        set(CPACK_PRODUCTBUILD_IDENTITY_NAME "$ENV{APPLE_INSTALLER_IDENTITY}")
        set(CPACK_PKGBUILD_IDENTITY_NAME "$ENV{APPLE_INSTALLER_IDENTITY}")
    endif()
endif()
