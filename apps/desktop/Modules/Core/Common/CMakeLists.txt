include(AedstCompilerWarnings)

find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)

# AedstCore: pure C++ (no Qt). Domain layers of every feature depend on this
# and nothing else, which keeps use cases testable with plain GoogleTest.
add_library(AedstCore STATIC
    include/aedst/core/error.hpp
    include/aedst/core/error_sink.hpp
    include/aedst/core/outcome.hpp
    include/aedst/core/use_case.hpp
    include/aedst/core/clock.hpp
    include/aedst/core/guarded.hpp
    include/aedst/core/logger.hpp
    include/aedst/core/correlation.hpp
    include/aedst/core/redaction.hpp
    include/aedst/core/secure_memory.hpp
    src/clock.cpp
    src/guarded.cpp
    src/logger.cpp
    src/redacting_sink.hpp
    src/correlation.cpp
    src/redaction.cpp
)
add_library(Aedst::Core ALIAS AedstCore)

target_include_directories(AedstCore PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(AedstCore PUBLIC cxx_std_23)

# PUBLIC: AedstLogger's variadic templates forward straight into spdlog::logger,
# so consumers compile against spdlog/fmt headers. No other module links spdlog.
target_link_libraries(AedstCore PUBLIC spdlog::spdlog fmt::fmt)

aedst_set_warnings(AedstCore)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
