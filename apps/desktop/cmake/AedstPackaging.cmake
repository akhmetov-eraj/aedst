include_guard(GLOBAL)

# Included from App/CMakeLists.txt after the aedst-desktop target and its
# install() rules exist. Sets the shared CPack metadata, pulls in the
# per-platform module, then include(CPack) — so `cpack` / the `package` target
# work from any configured build tree.

set(AEDST_PACKAGING_DIR "${PROJECT_SOURCE_DIR}/packaging")
set(AEDST_APP_ID "com.aedst.Desktop")
set(AEDST_DISPLAY_NAME "AEDST")

set(CPACK_PACKAGE_NAME "${AEDST_DISPLAY_NAME}")
set(CPACK_PACKAGE_VENDOR "AEDST")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${AEDST_DISPLAY_NAME}")
set(CPACK_PACKAGE_EXECUTABLES "aedst-desktop;${AEDST_DISPLAY_NAME}")
set(CPACK_PACKAGE_FILE_NAME "aedst-desktop-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}")
set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_BINARY_DIR}/packages")
set(CPACK_STRIP_FILES ON)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    include("${CMAKE_CURRENT_LIST_DIR}/packaging/Windows.cmake")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include("${CMAKE_CURRENT_LIST_DIR}/packaging/Linux.cmake")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # DMG (DragNDrop) + PKG (productbuild); signing/notarization: packaging/macos/.
    include("${CMAKE_CURRENT_LIST_DIR}/packaging/macOS.cmake")
else()
    message(STATUS "AEDST: no platform packaging for ${CMAKE_SYSTEM_NAME}; falling back to a TGZ")
    set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)
