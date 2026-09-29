include_guard(GLOBAL)

# --- CPack NSIS (the path `cpack` and CI use) ---------------------------------
set(CPACK_GENERATOR "NSIS;ZIP")
set(CPACK_NSIS_DISPLAY_NAME "${AEDST_DISPLAY_NAME}")
set(CPACK_NSIS_PACKAGE_NAME "${AEDST_DISPLAY_NAME}")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set(CPACK_NSIS_MODIFY_PATH OFF)
set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\aedst-desktop.exe")
set(CPACK_NSIS_CREATE_ICONS_EXTRA
    "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\${AEDST_DISPLAY_NAME}.lnk' '$INSTDIR\\\\bin\\\\aedst-desktop.exe'")
set(CPACK_NSIS_DELETE_ICONS_EXTRA
    "Delete '$SMPROGRAMS\\\\$START_MENU\\\\${AEDST_DISPLAY_NAME}.lnk'")
# CPACK_NSIS_MUI_ICON / CPACK_NSIS_MUI_UNIICON: set once a real .ico exists
# under App/resources/icons (none is committed yet).

# --- Hand-maintained NSIS script (package_nsis) --------------------------------
# CPack's NSIS template covers the basics. packaging/windows/installer.nsi.in is
# the escape hatch for what the template can't express (file associations,
# custom pages, per-machine vs per-user choice). Keep exactly one of the two as
# the shipped path once the installer grows beyond the basics.
set(AEDST_NSIS_STAGING_DIR "${CMAKE_BINARY_DIR}/packaging/nsis/stage")
set(AEDST_NSIS_OUTPUT "${CMAKE_BINARY_DIR}/packages/aedst-desktop-${PROJECT_VERSION}-setup.exe")
file(TO_NATIVE_PATH "${AEDST_NSIS_STAGING_DIR}" AEDST_NSIS_STAGING_DIR_NATIVE)
file(TO_NATIVE_PATH "${AEDST_NSIS_OUTPUT}" AEDST_NSIS_OUTPUT_NATIVE)
configure_file("${AEDST_PACKAGING_DIR}/windows/installer.nsi.in"
    "${CMAKE_BINARY_DIR}/packaging/nsis/installer.nsi" @ONLY)

find_program(AEDST_MAKENSIS makensis PATHS "$ENV{ProgramFiles\(x86\)}/NSIS" "$ENV{ProgramFiles}/NSIS")
if(AEDST_MAKENSIS)
    add_custom_target(package_nsis
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${AEDST_NSIS_STAGING_DIR}"
        COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}" --prefix "${AEDST_NSIS_STAGING_DIR}" --config $<CONFIG>
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/packages"
        COMMAND "${AEDST_MAKENSIS}" /V2 "${CMAKE_BINARY_DIR}/packaging/nsis/installer.nsi"
        DEPENDS aedst-desktop
        COMMENT "Building NSIS installer from packaging/windows/installer.nsi.in"
        VERBATIM)
endif()

# --- MSIX (package_msix) -------------------------------------------------------
# Scope cut: the resulting .msix is UNSIGNED. Real distribution needs a
# code-signing certificate whose subject exactly matches Publisher in
# AppxManifest.xml, then `signtool sign /fd SHA256 /f <cert.pfx> <file>.msix`.
# Unsigned packages only install with Developer Mode on
# (`Add-AppxPackage -AllowUnsigned`, Windows 11). Not wired into CI.
set(AEDST_MSIX_PUBLISHER "CN=AEDST Development" CACHE STRING "MSIX Identity Publisher (must match the signing cert subject)")
set(AEDST_MSIX_VERSION "${PROJECT_VERSION}.0") # MSIX requires four components
set(AEDST_MSIX_DIR "${CMAKE_BINARY_DIR}/packaging/msix")
configure_file("${AEDST_PACKAGING_DIR}/windows/AppxManifest.xml.in" "${AEDST_MSIX_DIR}/AppxManifest.xml" @ONLY)

find_program(AEDST_MAKEAPPX makeappx)
find_program(AEDST_MAKEPRI makepri)
if(AEDST_MAKEAPPX AND AEDST_MAKEPRI)
    add_custom_target(package_msix
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${AEDST_MSIX_DIR}/root"
        COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}" --prefix "${AEDST_MSIX_DIR}/root" --config $<CONFIG>
        COMMAND "${CMAKE_COMMAND}" -E copy "${AEDST_MSIX_DIR}/AppxManifest.xml" "${AEDST_MSIX_DIR}/root/AppxManifest.xml"
        COMMAND "${AEDST_MAKEPRI}" createconfig /cf "${AEDST_MSIX_DIR}/priconfig.xml" /dq en-US /o
        COMMAND "${AEDST_MAKEPRI}" new /pr "${AEDST_MSIX_DIR}/root" /cf "${AEDST_MSIX_DIR}/priconfig.xml"
                /of "${AEDST_MSIX_DIR}/root/resources.pri" /o
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/packages"
        COMMAND "${AEDST_MAKEAPPX}" pack /d "${AEDST_MSIX_DIR}/root"
                /p "${CMAKE_BINARY_DIR}/packages/aedst-desktop-${PROJECT_VERSION}.msix" /o
        DEPENDS aedst-desktop
        COMMENT "Building unsigned MSIX (see scope-cut note in cmake/packaging/Windows.cmake)"
        VERBATIM)
else()
    message(STATUS "AEDST: makeappx/makepri not found (Windows SDK); package_msix target disabled")
endif()
