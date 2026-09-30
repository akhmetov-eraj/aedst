include_guard(GLOBAL)

# Fail at configure time, with an actionable message, instead of deep inside a
# std::expected error cascade. The tree uses C++23 <expected> (Outcome/ApiResult),
# std::type_identity (C++20) and designated initialisers; nothing newer.
#
# Minimums: MSVC 19.37 (VS 2022 17.7), GCC 13, Clang 17 (with libstdc++ 13 or
# libc++ 17 - Clang has no <expected> of its own).

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 19.37)
    message(FATAL_ERROR "MSVC ${CMAKE_CXX_COMPILER_VERSION} is too old; need 19.37+ (VS 2022 17.7).")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 13)
    message(FATAL_ERROR "GCC ${CMAKE_CXX_COMPILER_VERSION} is too old; need 13+.")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 17)
    message(FATAL_ERROR "Clang ${CMAKE_CXX_COMPILER_VERSION} is too old; need 17+.")
endif()

include(CheckCXXSourceCompiles)
# CMAKE_CXX_STANDARD is honoured by check_cxx_source_compiles (CMP0067 NEW).
check_cxx_source_compiles("
#include <expected>
#include <span>
#include <string>
#include <type_traits>
#if !defined(__cpp_lib_expected) || __cpp_lib_expected < 202211L
#error \"std::expected missing\"
#endif
struct Err { std::string message; };
std::expected<int, Err> f(bool ok) { if (ok) { return 1; } return std::unexpected(Err{\"x\"}); }
int main() { auto r = f(true); return r.value_or(0) == 1 ? 0 : 1; }
" AEDST_HAS_STD_EXPECTED)

if(NOT AEDST_HAS_STD_EXPECTED)
    message(FATAL_ERROR "The C++ toolchain does not provide std::expected (C++23). "
                        "Use MSVC 19.37+, GCC 13+, or Clang 17+ with libstdc++ 13+/libc++ 17+.")
endif()
