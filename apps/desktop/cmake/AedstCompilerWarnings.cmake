include_guard(GLOBAL)

include(AedstHardening)

# aedst_set_warnings(<target>)
#
# Strict, per-compiler warning set applied to every first-party target, plus the exploit-mitigation flags of
# aedst_set_hardening (cmake/AedstHardening.cmake): every first-party target already calls this function.
# INTERFACE libraries get the flags as INTERFACE options so header-only code
# is checked in the consumers that compile it.
function(aedst_set_warnings target)
    set(msvc_warnings
        /W4
        /permissive-
        /utf-8
        /Zc:__cplusplus
        /w14242 # conversion, possible loss of data
        /w14254 # larger bit field to smaller bit field
        /w14263 # member function hides a virtual base function
        /w14265 # class has virtual functions but non-virtual destructor
        /w14287 # unsigned/negative constant mismatch
        /we4289 # loop control variable used outside the loop
        /w14296 # expression is always true/false
        /w14311 # pointer truncation
        /w14545 /w14546 /w14547 /w14549 /w14555 # suspicious comma/expression forms
        /w14640 # thread-unsafe static member initialization
        /w14826 # sign-extended conversion
        /w14905 /w14906 # string literal casts
        /w14928 # illegal copy-initialization
    )

    set(clang_warnings
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wconversion
        -Wsign-conversion
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
        -Wimplicit-fallthrough
    )

    set(gcc_warnings
        ${clang_warnings}
        -Wmisleading-indentation
        -Wduplicated-cond
        -Wduplicated-branches
        -Wlogical-op
    )

    if(AEDST_WARNINGS_AS_ERRORS)
        list(APPEND msvc_warnings /WX)
        list(APPEND clang_warnings -Werror)
        list(APPEND gcc_warnings -Werror)
    endif()

    get_target_property(target_type ${target} TYPE)
    if(target_type STREQUAL "INTERFACE_LIBRARY")
        set(scope INTERFACE)
    else()
        set(scope PRIVATE)
    endif()

    # Quoted so the ;-separated lists stay inside each generator expression.
    target_compile_options(${target} ${scope}
        "$<$<CXX_COMPILER_ID:MSVC>:${msvc_warnings}>"
        "$<$<CXX_COMPILER_ID:GNU>:${gcc_warnings}>"
        "$<$<CXX_COMPILER_ID:Clang>:${clang_warnings}>"
    )

    aedst_set_hardening(${target})
endfunction()
