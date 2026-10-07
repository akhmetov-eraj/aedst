include_guard(GLOBAL)

# QtKeychain: the OS credential store for the OIDC refresh token (Windows Credential Manager,
# Linux Secret Service / libsecret / KWallet). BSD-3-Clause, https://github.com/frankosterfeld/qtkeychain.
#
# It is built from source as a pinned FetchContent subproject, NOT through vcpkg: the vcpkg port
# would pull vcpkg's own Qt, while this tree deliberately uses Qt's official binaries
# (docs/adr/0002-dependency-management.md). Version 0.17.0 was the latest release on 2026-07-06
# (verified against the GitHub releases API); the tag is annotated, so the commit is the tag's target.
#
# The subproject is EXCLUDE_FROM_ALL so its install() rules never end up in our packages, and it
# is linked statically (no extra shared library to ship or to replace).
#
# Provides AEDST_QTKEYCHAIN_TARGET (a CMake target) and AEDST_QTKEYCHAIN_INCLUDE_DIRS.
function(aedst_fetch_qtkeychain)
    include(FetchContent)
    if(POLICY CMP0169)
        cmake_policy(SET CMP0169 OLD) # single-argument FetchContent_Populate: needed for EXCLUDE_FROM_ALL on 3.25
    endif()

    FetchContent_Declare(qtkeychain
        GIT_REPOSITORY https://github.com/frankosterfeld/qtkeychain.git
        GIT_TAG 0.17.0
        GIT_SHALLOW TRUE)

    FetchContent_GetProperties(qtkeychain)
    if(NOT qtkeychain_POPULATED)
        FetchContent_Populate(qtkeychain)

        # Directory-scope settings: they only affect the subproject added below.
        set(BUILD_WITH_QT5 OFF)
        set(BUILD_TEST_APPLICATION OFF)
        set(BUILD_QTQUICK_DEMO OFF)
        set(BUILD_TRANSLATIONS OFF)
        set(BUILD_SHARED_LIBS OFF)
        add_subdirectory("${qtkeychain_SOURCE_DIR}" "${qtkeychain_BINARY_DIR}" EXCLUDE_FROM_ALL)
    endif()

    if(TARGET qt6keychain)
        set(target qt6keychain)
    elseif(TARGET Qt6Keychain::Qt6Keychain)
        set(target Qt6Keychain::Qt6Keychain)
    else()
        message(FATAL_ERROR "QtKeychain was fetched but no Qt 6 target was defined (expected qt6keychain).")
    endif()

    set(AEDST_QTKEYCHAIN_TARGET "${target}" PARENT_SCOPE)
    # keychain.h sits at the top of the source tree, the generated export header in the binary tree.
    set(AEDST_QTKEYCHAIN_INCLUDE_DIRS "${qtkeychain_SOURCE_DIR}" "${qtkeychain_BINARY_DIR}" PARENT_SCOPE)
endfunction()
