include_guard(GLOBAL)

# Velopack C/C++ SDK (MIT, https://github.com/velopack/velopack): update download and install for Windows and
# Linux AppImage installs. Opt-in (-DAEDST_ENABLE_VELOPACK=ON, used by the CI release builds): without it the updater
# still verifies the signed manifest and reports updates, but a install cannot replace itself.
#
# Distribution: a prebuilt velopack_libc zip from GitHub Releases (the SDK is not on vcpkg). The version and the SHA-256 are
# pinned here; the SHA-256 is the digest GitHub's releases API reports for the asset, and CMake refuses a download that does
# not match (URL_HASH). To bump: read the new tag's asset digest from
#   https://api.github.com/repos/velopack/velopack/releases/tags/<tag>
# change both values together, and re-check the C++ names used in Modules/Core/Updater/src/velopack_installer.cpp.
#
# Provides AEDST_VELOPACK_INCLUDE_DIR and AEDST_VELOPACK_LIBRARY (or stops with an actionable error).
set(AEDST_VELOPACK_VERSION "1.2.161")
set(AEDST_VELOPACK_SHA256 "31de8f2cf05a47bd5328a9f98b0c41aec1582156f6075f1058b26941f7f17c20")

function(aedst_fetch_velopack)
    include(FetchContent)
    if(DEFINED AEDST_VELOPACK_ROOT AND EXISTS "${AEDST_VELOPACK_ROOT}")
        set(root "${AEDST_VELOPACK_ROOT}") # an unpacked SDK supplied by the builder (offline builds, Flatpak)
    else()
        FetchContent_Declare(velopack
            URL "https://github.com/velopack/velopack/releases/download/${AEDST_VELOPACK_VERSION}/velopack_libc_${AEDST_VELOPACK_VERSION}.zip"
            URL_HASH "SHA256=${AEDST_VELOPACK_SHA256}"
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
        FetchContent_MakeAvailable(velopack)
        set(root "${velopack_SOURCE_DIR}")
    endif()

    find_path(include_dir NAMES Velopack.hpp Velopack.h
        PATHS "${root}" PATH_SUFFIXES include NO_DEFAULT_PATH)
    if(WIN32)
        set(library_names velopack_libc.dll.lib velopack_libc velopack_libc_win_x64_msvc)
    else()
        set(library_names velopack_libc libvelopack_libc)
    endif()
    find_library(library NAMES ${library_names}
        PATHS "${root}" PATH_SUFFIXES lib lib/shared lib/static lib/x64 lib/linux-x64 NO_DEFAULT_PATH)
    if(NOT include_dir OR NOT library)
        message(FATAL_ERROR
            "Velopack SDK ${AEDST_VELOPACK_VERSION} was unpacked to '${root}' but its header (Velopack.hpp) or library "
            "(velopack_libc) was not found where cmake/AedstVelopack.cmake looks (include/, lib/, lib/shared, lib/static). "
            "Check the layout of the zip and adjust PATH_SUFFIXES, or pass -DAEDST_VELOPACK_ROOT=<unpacked SDK>.")
    endif()
    # The shared runtime (velopack_libc.dll / libvelopack_libc.so) has to ship next to the executable. UNVERIFIED path
    # suffixes: adjust together with the lines above when the zip layout is inspected on the first real build.
    find_file(runtime NAMES velopack_libc.dll libvelopack_libc.so
        PATHS "${root}" PATH_SUFFIXES lib lib/shared bin lib/x64 lib/linux-x64 NO_DEFAULT_PATH)
    set(AEDST_VELOPACK_RUNTIME "${runtime}" PARENT_SCOPE)
    set(AEDST_VELOPACK_INCLUDE_DIR "${include_dir}" PARENT_SCOPE)
    set(AEDST_VELOPACK_LIBRARY "${library}" PARENT_SCOPE)
endfunction()
