include_guard(GLOBAL)

# Included BEFORE project(): compiler launchers and the MSVC debug-info format
# are read when the first target/compiler is set up.
#
# A compiler cache is the biggest build-time win available for this tree
# (protoc-generated code and the Qt-heavy TUs recompile identically across
# branches). sccache handles MSVC and GCC/Clang; ccache is the Linux fallback.
option(AEDST_USE_COMPILER_CACHE "Use sccache/ccache as compiler launcher when found" ON)

if(AEDST_USE_COMPILER_CACHE AND NOT CMAKE_CXX_COMPILER_LAUNCHER)
    find_program(AEDST_SCCACHE_EXECUTABLE sccache)
    find_program(AEDST_CCACHE_EXECUTABLE ccache)
    if(AEDST_SCCACHE_EXECUTABLE)
        set(CMAKE_C_COMPILER_LAUNCHER "${AEDST_SCCACHE_EXECUTABLE}")
        set(CMAKE_CXX_COMPILER_LAUNCHER "${AEDST_SCCACHE_EXECUTABLE}")
    elseif(AEDST_CCACHE_EXECUTABLE)
        set(CMAKE_C_COMPILER_LAUNCHER "${AEDST_CCACHE_EXECUTABLE}")
        set(CMAKE_CXX_COMPILER_LAUNCHER "${AEDST_CCACHE_EXECUTABLE}")
    endif()
    if(CMAKE_CXX_COMPILER_LAUNCHER)
        # Compiler caches cannot cache MSVC's shared PDB (/Zi); embed debug info (/Z7)
        # instead. CMP0141 is NEW under cmake_minimum_required(3.25). Ignored by GCC/Clang.
        set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")
    endif()
endif()
