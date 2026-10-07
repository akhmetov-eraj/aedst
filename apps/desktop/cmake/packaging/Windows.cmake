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

# Uninstaller: CPack's template already removes every installed file, the install
# directory and the Start Menu folder/shortcuts, and the Add/Remove Programs entry.
# On top of that: an OPTIONAL prompt to delete per-user data, defaulting to NO (and to NO
# in silent mode, /SD IDNO, so unattended uninstalls never destroy user data).
# User data locations (QStandardPaths AppDataLocation/CacheLocation for org+app "AEDST"):
# %APPDATA%\AEDST and %LOCALAPPDATA%\AEDST. Keep in sync with QCoreApplication
# organization/application names set in App/src/main.cpp.
set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS
"  MessageBox MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2 'Also delete your AEDST user data (settings, local database, cache)?' /SD IDNO IDNO aedst_keep_user_data
  SetShellVarContext current
  RMDir /r '$APPDATA\\\\AEDST'
  RMDir /r '$LOCALAPPDATA\\\\AEDST'
  SetShellVarContext all
  aedst_keep_user_data:")

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
# Optional signing: `sign_msix` (below) signs with packaging/windows/sign.ps1 when
# AEDST_MSIX_SIGN=ON and WINDOWS_CERT_PFX_BASE64 / WINDOWS_CERT_PASSWORD (or a
# certificate already in the store, see the script) are available.
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

# --- Signing (sign_msix) --------------------------------------------------------
# packaging/windows/sign.ps1 wraps signtool (SHA-256 file digest + RFC 3161 timestamp).
# NSIS installer / exe signing is done by CI (or by hand) with the same script; see
# packaging/README.md.
option(AEDST_MSIX_SIGN "Add a sign_msix target that signs the .msix with packaging/windows/sign.ps1" OFF)
if(AEDST_MSIX_SIGN AND TARGET package_msix)
    find_program(AEDST_POWERSHELL NAMES pwsh powershell REQUIRED)
    add_custom_target(sign_msix
        COMMAND "${AEDST_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass
                -File "${AEDST_PACKAGING_DIR}/windows/sign.ps1"
                -Files "${CMAKE_BINARY_DIR}/packages/aedst-desktop-${PROJECT_VERSION}.msix"
        DEPENDS package_msix
        USES_TERMINAL
        COMMENT "Signing the MSIX (publisher in AppxManifest must match the certificate subject)"
        VERBATIM)
endif()

# --- MSI (package_msi) ---------------------------------------------------------------
# Enterprise deployment (GPO, Intune, SCCM) wants an MSI next to the NSIS setup: per-machine, silent
# (`msiexec /i aedst.msi /qn`), upgradable in place. CPack's WIX generator builds it (WiX Toolset: candle/light v3 or the
# `wix` CLI v4+; whichever is on PATH). The MSI is not the self-updating channel (that is the Velopack setup); it is
# updated by the management tool that deployed it, with a newer MSI sharing this UpgradeCode.
# Sign the produced .msi with packaging/windows/sign.ps1.
# CPACK_WIX_UPGRADE_GUID identifies the product line for ever: never change it, or upgrades install side by side.
set(CPACK_WIX_UPGRADE_GUID "7668C1B6-78CD-4AE9-ADA2-B6CF0F06507C")
set(CPACK_WIX_PROGRAM_MENU_FOLDER "${AEDST_DISPLAY_NAME}")
set(CPACK_WIX_CMAKE_PACKAGE_REGISTRY "AEDST")
find_program(AEDST_WIX_CANDLE candle PATHS "$ENV{WIX}/bin")
find_program(AEDST_WIX_CLI wix)
if(AEDST_WIX_CANDLE OR AEDST_WIX_CLI)
    add_custom_target(package_msi
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/packages"
        COMMAND "${CMAKE_CPACK_COMMAND}" -G WIX -C $<CONFIG> --config "${CMAKE_BINARY_DIR}/CPackConfig.cmake"
                -B "${CMAKE_BINARY_DIR}/packages"
        DEPENDS aedst-desktop
        USES_TERMINAL
        COMMENT "Building the MSI with CPack WIX"
        VERBATIM)
else()
    message(STATUS "AEDST: WiX Toolset not found; package_msi target disabled")
endif()
