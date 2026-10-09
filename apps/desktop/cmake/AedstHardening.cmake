include_guard(GLOBAL)

# aedst_set_hardening(<target>)
#
# Exploit-mitigation flags for every first-party target (called from aedst_set_warnings, so each target that has the
# warning set also has these). Compile options go on every target type, link options only on things that are linked
# into a final binary (executables, shared/module libraries); a static library's objects are hardened at compile time
# and the final link flags come from the executable that consumes them.
#
#   MSVC:
#     compile  /guard:cf (Control Flow Guard) /sdl (extra security checks; /GS is already the default)
#     link     /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT /CETCOMPAT /guard:cf  (+ /INCREMENTAL:NO: CFG cannot be incremental)
#   GCC / Clang (Linux):
#     compile  -fstack-protector-strong, -fstack-clash-protection, -fcf-protection=full (x86 only), PIE
#              (POSITION_INDEPENDENT_CODE, which CMake turns into -fPIE / -pie), -D_FORTIFY_SOURCE=3 in optimised builds,
#              -D_GLIBCXX_ASSERTIONS in Debug (libstdc++ bounds/precondition checks)
#     link     -Wl,-z,relro,-z,now,-z,noexecstack
#
# Skipped on purpose: _FORTIFY_SOURCE under a sanitizer or coverage build (ASan intercepts the same functions and
# FORTIFY needs optimisation); every flag the compiler does not know (probed with check_cxx_compiler_flag), so an
# unusual architecture or old compiler degrades to "not hardened" with a configure-time STATUS line, never to a failure.
# `scripts/verify-install-*` checks the shipped binary (winchecksec / checksec), so a silently missing flag is caught.
#
# AEDST_ENABLE_HARDENING=OFF turns the whole thing off (only for diagnosing a toolchain problem; CI keeps it ON).

option(AEDST_ENABLE_HARDENING "Build first-party targets with exploit-mitigation flags (CFG, stack protector, FORTIFY, RELRO, PIE...)" ON)

include(CheckCXXCompilerFlag)
include(CheckLinkerFlag)
include(CheckPIESupported)

set(_aedst_hardening_probed FALSE)

# Probes the GCC/Clang flags once and stores the ones that work in AEDST_HARDEN_COMPILE_FLAGS / AEDST_HARDEN_LINK_FLAGS.
macro(_aedst_probe_hardening_flags)
    set(AEDST_HARDEN_COMPILE_FLAGS "")
    set(AEDST_HARDEN_LINK_FLAGS "")
    foreach(flag IN ITEMS -fstack-protector-strong -fstack-clash-protection -fcf-protection=full)
        string(MAKE_C_IDENTIFIER "AEDST_HAS${flag}" var)
        check_cxx_compiler_flag("${flag}" ${var})
        if(${var})
            list(APPEND AEDST_HARDEN_COMPILE_FLAGS "${flag}")
        else()
            message(STATUS "AEDST hardening: compiler does not accept ${flag}; skipped")
        endif()
    endforeach()
    foreach(flag IN ITEMS "-Wl,-z,relro" "-Wl,-z,now" "-Wl,-z,noexecstack")
        string(MAKE_C_IDENTIFIER "AEDST_HAS_LINK${flag}" var)
        check_linker_flag(CXX "${flag}" ${var})
        if(${var})
            list(APPEND AEDST_HARDEN_LINK_FLAGS "${flag}")
        else()
            message(STATUS "AEDST hardening: linker does not accept ${flag}; skipped")
        endif()
    endforeach()
    check_pie_supported(LANGUAGES CXX)
    set(_aedst_hardening_probed TRUE)
endmacro()

function(aedst_set_hardening target)
    if(NOT AEDST_ENABLE_HARDENING)
        return()
    endif()

    get_target_property(target_type ${target} TYPE)
    if(target_type STREQUAL "INTERFACE_LIBRARY")
        return() # no sources of its own; the consumers that compile its headers are hardened
    endif()
    set(links FALSE)
    if(target_type MATCHES "^(EXECUTABLE|SHARED_LIBRARY|MODULE_LIBRARY)$")
        set(links TRUE)
    endif()

    if(MSVC)
        target_compile_options(${target} PRIVATE /guard:cf /sdl)
        if(links)
            target_link_options(${target} PRIVATE /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT /guard:cf /INCREMENTAL:NO)
            if(CMAKE_SIZEOF_VOID_P EQUAL 8)
                target_link_options(${target} PRIVATE /CETCOMPAT) # x86/x64 shadow stacks; not valid for ARM64 links
            endif()
        endif()
        return()
    endif()

    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        return()
    endif()
    if(NOT _aedst_hardening_probed)
        _aedst_probe_hardening_flags()
    endif()

    target_compile_options(${target} PRIVATE ${AEDST_HARDEN_COMPILE_FLAGS})
    set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)

    # FORTIFY needs optimisation and fights sanitizers; the glibc/libstdc++ assertions are cheap Debug checks.
    if(AEDST_SANITIZER STREQUAL "none" AND NOT AEDST_COVERAGE)
        target_compile_options(${target} PRIVATE
            "$<$<NOT:$<CONFIG:Debug>>:-U_FORTIFY_SOURCE>"
            "$<$<NOT:$<CONFIG:Debug>>:-D_FORTIFY_SOURCE=3>")
    endif()
    target_compile_definitions(${target} PRIVATE "$<$<CONFIG:Debug>:_GLIBCXX_ASSERTIONS>")

    if(links)
        target_link_options(${target} PRIVATE ${AEDST_HARDEN_LINK_FLAGS})
    endif()
endfunction()
