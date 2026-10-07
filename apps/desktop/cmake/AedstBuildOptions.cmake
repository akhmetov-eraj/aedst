include_guard(GLOBAL)

# Build-wide performance and toolchain settings. Included after project() and
# before any add_subdirectory(), so every target picks them up at creation.
#
# Deliberately NOT here (measure first, then add with numbers in the PR):
#  - -O3 / -Ofast: CMake's Release default (/O2, -O3 on GCC/Clang) is kept.
#  - PCH / unity builds: opt in with -DCMAKE_UNITY_BUILD=ON or
#    -DCMAKE_PRECOMPILE_HEADERS; not on by default because they hide missing
#    includes that the module-boundary rule relies on the compiler to catch.
#  - mimalloc: see docs/performance.md for the adoption criteria.

option(AEDST_ENABLE_LTO "Link-time optimisation for Release builds (skipped if the toolchain lacks it)" ON)
option(AEDST_FAST_LINKER "Use mold, else lld, when available (GCC/Clang on Linux)" ON)

# --- LTO / IPO ---------------------------------------------------------------
if(AEDST_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT aedst_ipo_supported OUTPUT aedst_ipo_message LANGUAGES CXX)
    if(aedst_ipo_supported)
        # Per-config variable: initialises INTERPROCEDURAL_OPTIMIZATION_RELEASE on every
        # target created afterwards. Debug builds keep fast incremental links.
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    else()
        message(STATUS "AEDST: LTO not available (${aedst_ipo_message})")
    endif()
endif()

# --- Linker ------------------------------------------------------------------
if(AEDST_FAST_LINKER AND CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    find_program(AEDST_MOLD_EXECUTABLE mold)
    find_program(AEDST_LLD_EXECUTABLE NAMES ld.lld lld)
    if(AEDST_MOLD_EXECUTABLE)
        add_link_options(-fuse-ld=mold)
        message(STATUS "AEDST: linking with mold")
    elseif(AEDST_LLD_EXECUTABLE)
        add_link_options(-fuse-ld=lld)
        message(STATUS "AEDST: linking with lld")
    endif()
endif()

# --- Dead-code elimination (GCC/Clang) -------------------------------------------
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options("$<$<CONFIG:Release>:-ffunction-sections>" "$<$<CONFIG:Release>:-fdata-sections>")
    add_link_options("$<$<CONFIG:Release>:-Wl,--gc-sections>")
endif()

# --- Never ship the QML debugger ----------------------------------------------
# Qt Creator kits pass -DQT_QML_DEBUG for Debug builds; it opens a debug port and
# disables QML compiler optimisations. Fail loudly if it leaks into a shippable config.
if(CMAKE_BUILD_TYPE MATCHES "^(Release|MinSizeRel)$" AND
   ("${CMAKE_CXX_FLAGS}" MATCHES "QT_QML_DEBUG" OR "${CMAKE_CXX_FLAGS_RELEASE}" MATCHES "QT_QML_DEBUG"))
    message(FATAL_ERROR "QT_QML_DEBUG must not be defined in ${CMAKE_BUILD_TYPE} builds "
                        "(set the Qt Creator kit's 'QML debugging' to Disable).")
endif()

# --- Release debug symbols (shipped stripped, symbols kept as a CI artifact) ------
# Crash dumps from the field are useless without symbols that match the shipped
# binary. Release builds therefore carry debug info that is SPLIT OFF after linking:
#   MSVC        /Z7 (info embedded in objects; keeps sccache working) + link /DEBUG
#               /OPT:REF /OPT:ICF -> aedst-desktop.pdb, copied to <build>/symbols
#   GCC/Clang   -g1 (line tables + inlines, small) ; objcopy --only-keep-debug ->
#               <build>/symbols/aedst-desktop.debug (matched by --build-id)
# The split target is created by aedst_split_debug_symbols() (called from App/).
option(AEDST_RELEASE_DEBUG_SYMBOLS "Emit and split off debug symbols for Release builds" ON)

if(AEDST_RELEASE_DEBUG_SYMBOLS)
    if(MSVC)
        add_compile_options("$<$<CONFIG:Release>:/Z7>")
        # /DEBUG turns off /OPT:REF and /OPT:ICF unless they are restated.
        # /PDBALTPATH keeps the build machine's directory layout out of the shipped exe.
        add_link_options("$<$<CONFIG:Release>:/DEBUG>" "$<$<CONFIG:Release>:/OPT:REF>"
                         "$<$<CONFIG:Release>:/OPT:ICF>" "$<$<CONFIG:Release>:/PDBALTPATH:%_PDB%>")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options("$<$<CONFIG:Release>:-g1>")
        add_link_options("$<$<CONFIG:Release>:-Wl,--build-id=sha1>")
    endif()
endif()

# aedst_split_debug_symbols(<target>)
#
# Adds an ALL target `aedst_split_debug_symbols` that copies/splits the symbols of
# <target> into ${CMAKE_BINARY_DIR}/symbols. No-op outside Release/RelWithDebInfo
# single-config builds or when AEDST_RELEASE_DEBUG_SYMBOLS is OFF. The split runs
# on the build-tree binary; CPack's strip (CPACK_STRIP_FILES) happens later on the
# installed copy, and --build-id / PDB GUID still match.
function(aedst_split_debug_symbols target)
    if(NOT AEDST_RELEASE_DEBUG_SYMBOLS OR NOT CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo)$")
        return()
    endif()
    set(sym_dir "${CMAKE_BINARY_DIR}/symbols")
    if(MSVC)
        add_custom_target(aedst_split_debug_symbols ALL
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${sym_dir}"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_PDB_FILE:${target}>" "${sym_dir}"
            DEPENDS ${target}
            COMMENT "Collecting PDB for ${target}"
            VERBATIM)
    else()
        find_program(AEDST_OBJCOPY objcopy)
        if(NOT AEDST_OBJCOPY)
            message(WARNING "AEDST: objcopy not found; debug symbols will not be split")
            return()
        endif()
        add_custom_target(aedst_split_debug_symbols ALL
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${sym_dir}"
            COMMAND "${AEDST_OBJCOPY}" --only-keep-debug "$<TARGET_FILE:${target}>" "${sym_dir}/$<TARGET_FILE_NAME:${target}>.debug"
            DEPENDS ${target}
            COMMENT "Splitting debug symbols of ${target}"
            VERBATIM)
    endif()
endfunction()
