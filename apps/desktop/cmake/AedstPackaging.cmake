include_guard(GLOBAL)

# Included from App/CMakeLists.txt after the aedst-desktop target and its
# install() rules exist. Sets the shared CPack metadata, pulls in the
# per-platform module, then include(CPack) — so `cpack` / the `package` target
# work from any configured build tree.

# Legal files travel with every package (docs/compliance.md): the LGPL/GPL texts always, and the generated
# THIRD_PARTY_NOTICES.txt when CI produced it before cpack (scripts/compliance/compliance.mjs notices --out <build>/compliance).
install(FILES "${PROJECT_SOURCE_DIR}/compliance/licenses/LGPL-3.0.txt" "${PROJECT_SOURCE_DIR}/compliance/licenses/GPL-3.0.txt"
    DESTINATION "${CMAKE_INSTALL_DOCDIR}/licenses")
install(FILES "${CMAKE_BINARY_DIR}/compliance/THIRD_PARTY_NOTICES.txt" "${CMAKE_BINARY_DIR}/compliance/sbom.spdx.json"
    DESTINATION "${CMAKE_INSTALL_DOCDIR}" OPTIONAL)

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
else()
    message(STATUS "AEDST: no platform packaging for ${CMAKE_SYSTEM_NAME}; falling back to a TGZ")
    set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)
