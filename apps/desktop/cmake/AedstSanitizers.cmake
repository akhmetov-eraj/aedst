include_guard(GLOBAL)

# Sanitizer + coverage instrumentation, applied GLOBALLY (add_compile_options /
# add_link_options) so first-party targets, tests and generated code are all
# instrumented alike. Must be included before any target is created (root
# CMakeLists.txt does this right after AedstBuildOptions).
#
#   -DAEDST_SANITIZER=none | asan-ubsan | tsan
#
# GCC/Clang: asan-ubsan = -fsanitize=address,undefined ; tsan = -fsanitize=thread
#            (ASan and TSan are mutually exclusive by design).
# MSVC:      only AddressSanitizer exists (/fsanitize=address, VS 2019 16.9+);
#            both `asan-ubsan` and `asan` request it, UBSan is not available.
#
# Instrumented vcpkg dependencies are NOT rebuilt with the sanitizer; only this
# tree is instrumented. Third-party noise goes through cmake/sanitizers/*.supp.
#
# Run-time environment (used by the *-asan / *-tsan test presets and by the
# check-sanitize target) is exported as AEDST_SANITIZER_RUN_ENV.

set(AEDST_SANITIZER "none" CACHE STRING "Sanitizer to build with: none, asan-ubsan, tsan")
set_property(CACHE AEDST_SANITIZER PROPERTY STRINGS none asan-ubsan tsan)
option(AEDST_SANITIZER_NO_RECOVER "Abort on the first UBSan finding instead of printing and continuing (CI wants ON)" ON)
option(AEDST_COVERAGE "Instrument for gcov/gcovr line+branch coverage (GCC/Clang, Debug, no LTO)" OFF)

set(AEDST_SANITIZER_SUPP_DIR "${CMAKE_CURRENT_LIST_DIR}/sanitizers")
set(AEDST_SANITIZER_RUN_ENV "")

if(NOT AEDST_SANITIZER MATCHES "^(none|asan-ubsan|asan|tsan)$")
    message(FATAL_ERROR "AEDST_SANITIZER='${AEDST_SANITIZER}' is invalid; use none, asan-ubsan or tsan.")
endif()

if(AEDST_SANITIZER AND NOT AEDST_SANITIZER STREQUAL "none")
    # Sanitizers and LTO do not mix (ThinLTO/-flto + instrumentation slows links
    # to a crawl and hides reports); refuse rather than silently drop one.
    if(CMAKE_INTERPROCEDURAL_OPTIMIZATION OR
       (CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE AND
        (CMAKE_CONFIGURATION_TYPES OR CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo|MinSizeRel)$")))
        message(FATAL_ERROR
            "AEDST_SANITIZER=${AEDST_SANITIZER} cannot be combined with LTO. "
            "Configure with -DAEDST_ENABLE_LTO=OFF (the *-asan / *-tsan presets are Debug and already LTO-free).")
    endif()
    if(AEDST_COVERAGE)
        message(FATAL_ERROR "AEDST_SANITIZER and AEDST_COVERAGE are mutually exclusive: run them in separate build trees.")
    endif()
endif()

if(AEDST_SANITIZER STREQUAL "none")
    # nothing to instrument
elseif(MSVC)
    if(AEDST_SANITIZER STREQUAL "tsan")
        message(FATAL_ERROR "MSVC has no ThreadSanitizer; use AEDST_SANITIZER=asan-ubsan (AddressSanitizer only) or a Clang preset.")
    endif()
    message(STATUS "AEDST: MSVC AddressSanitizer enabled (UBSan is not available on MSVC)")
    add_compile_options(/fsanitize=address)
    # ASan is incompatible with /RTC*, incremental linking and edit-and-continue.
    foreach(flag_var CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG)
        string(REGEX REPLACE "/RTC[1csu]+" "" ${flag_var} "${${flag_var}}")
    endforeach()
    add_link_options(/INCREMENTAL:NO)
    # Windows ASan: no LeakSanitizer; suppressions unsupported. Keep options minimal.
    list(APPEND AEDST_SANITIZER_RUN_ENV "ASAN_OPTIONS=strict_string_checks=1")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    if(AEDST_SANITIZER MATCHES "^(asan-ubsan|asan)$")
        set(aedst_san_flags -fsanitize=address,undefined)
        if(AEDST_SANITIZER_NO_RECOVER)
            list(APPEND aedst_san_flags -fno-sanitize-recover=undefined)
        endif()
        list(APPEND aedst_san_flags -fsanitize-address-use-after-scope)
        set(aedst_lsan "suppressions=${AEDST_SANITIZER_SUPP_DIR}/lsan.supp:print_suppressions=0")
        list(APPEND AEDST_SANITIZER_RUN_ENV
            "ASAN_OPTIONS=detect_leaks=1:strict_string_checks=1:detect_stack_use_after_return=1"
            "LSAN_OPTIONS=${aedst_lsan}"
            "UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1:suppressions=${AEDST_SANITIZER_SUPP_DIR}/ubsan.supp")
    else() # tsan
        set(aedst_san_flags -fsanitize=thread)
        list(APPEND AEDST_SANITIZER_RUN_ENV
            "TSAN_OPTIONS=second_deadlock_stack=1:halt_on_error=1:suppressions=${AEDST_SANITIZER_SUPP_DIR}/tsan.supp")
    endif()
    message(STATUS "AEDST: sanitizer '${AEDST_SANITIZER}' enabled (${aedst_san_flags})")
    # Frame pointers + some optimisation-free debug info give usable stack traces.
    add_compile_options(${aedst_san_flags} -fno-omit-frame-pointer -g)
    add_link_options(${aedst_san_flags})
else()
    message(FATAL_ERROR "AEDST_SANITIZER is not supported for compiler '${CMAKE_CXX_COMPILER_ID}'.")
endif()

# --- Coverage (gcovr; see Tests/CMakeLists.txt `coverage` target) --------------
if(AEDST_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "AEDST_COVERAGE needs GCC or Clang (gcov-compatible); got ${CMAKE_CXX_COMPILER_ID}.")
    endif()
    if(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE AND CMAKE_BUILD_TYPE STREQUAL "Release")
        message(FATAL_ERROR "AEDST_COVERAGE is meaningless with an optimised LTO Release build; use the `coverage` preset (Debug).")
    endif()
    message(STATUS "AEDST: coverage instrumentation enabled (--coverage)")
    add_compile_options(--coverage -O0 -g)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(-fprofile-update=atomic) # tests run threads (QThread/std::thread)
    endif()
    add_link_options(--coverage)
endif()
