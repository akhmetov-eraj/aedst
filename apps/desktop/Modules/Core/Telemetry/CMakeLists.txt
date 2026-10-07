include(AedstCompilerWarnings)

find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Core Network)

# Error/crash reporting and product analytics, behind the ports features already
# use (core::ErrorSink, analytics::AnalyticsTracker). See
# docs/adr/0022-telemetry-media-notifications.md.
#  - SentryReporter: sentry-native (crashpad backend), consent-gated, user id only.
#  - installQtLogBridge: qInstallMessageHandler -> AedstLogger (spdlog), warnings+.
#  - ErrorCenter: typed core::Error -> UI kind + toast/banner signal + breadcrumb.
#  - PostHogAnalyticsTracker: PostHog HTTP capture + feature flags over Qt Network.
#  - TelemetrySettings: the two opt-in switches QML binds to.
add_library(AedstTelemetry STATIC
    include/aedst/telemetry/error_center.hpp
    include/aedst/telemetry/posthog_analytics_tracker.hpp
    include/aedst/telemetry/qt_log_bridge.hpp
    include/aedst/telemetry/sentry_reporter.hpp
    include/aedst/telemetry/telemetry_settings.hpp
    src/error_center.cpp
    src/posthog_analytics_tracker.cpp
    src/qt_log_bridge.cpp
    src/sentry_reporter.cpp
    src/telemetry_settings.cpp
)
add_library(Aedst::Telemetry ALIAS AedstTelemetry)

target_include_directories(AedstTelemetry PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

# PUBLIC: ErrorCenter implements core::ErrorSink, the tracker implements
# analytics::AnalyticsTracker and is a QObject owning a QNetworkAccessManager.
target_link_libraries(AedstTelemetry
    PUBLIC
        Aedst::Core
        Aedst::Analytics
        Qt6::Core
        Qt6::Network
    PRIVATE
        Aedst::Network        # validateEndpoint (https policy for the PostHog host)
        Aedst::CrashReporting # ConsentStore
)

if(AEDST_ENABLE_SENTRY)
    find_package(sentry CONFIG REQUIRED)
    target_link_libraries(AedstTelemetry PRIVATE sentry::sentry)
    target_compile_definitions(AedstTelemetry PRIVATE AEDST_WITH_SENTRY)
    message(STATUS "AedstTelemetry: sentry-native enabled")
else()
    message(STATUS "AedstTelemetry: sentry-native disabled (AEDST_ENABLE_SENTRY=OFF); SentryReporter is a no-op")
endif()

if(WIN32)
    target_compile_definitions(AedstTelemetry PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

aedst_set_warnings(AedstTelemetry)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
