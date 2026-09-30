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

# --- Dead-code elimination (GCC/Clang, non-Apple) ------------------------------
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT APPLE)
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
