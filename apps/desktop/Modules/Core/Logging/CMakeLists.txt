include(AedstCompilerWarnings)

find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Core)

# Qt logging categories (lcApp, lcNetwork, ...) for Qt-aware modules. A leaf module:
# Qt Core only, no dependency on Aedst::Core or Aedst::Telemetry, so Network, Platform,
# Media and Navigation can log through categories without a dependency cycle. The sink
# (qInstallMessageHandler -> AedstLogger/spdlog) lives in Aedst::Telemetry.
add_library(AedstLogging STATIC
    include/aedst/logging/log_categories.hpp
    src/log_categories.cpp
)
add_library(Aedst::Logging ALIAS AedstLogging)

target_include_directories(AedstLogging PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_link_libraries(AedstLogging PUBLIC Qt6::Core)

aedst_set_warnings(AedstLogging)
