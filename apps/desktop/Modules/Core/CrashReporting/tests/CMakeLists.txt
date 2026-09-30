find_package(GTest REQUIRED)
include(GoogleTest)

# Non-signal logic only (consent gating, rate limiting, size limits, pending
# list, scrubbing, file naming). The signal/dump paths are verified manually.
add_executable(AedstCrashReportingTests
    consent_store_test.cpp
    crash_handler_naming_test.cpp
    crash_report_test.cpp
    crash_uploader_test.cpp
)
target_link_libraries(AedstCrashReportingTests PRIVATE Aedst::CrashReporting Aedst::Testing GTest::gtest_main)
aedst_set_warnings(AedstCrashReportingTests)

gtest_discover_tests(AedstCrashReportingTests
    TEST_PREFIX "AedstCrashReporting."
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS "unit")
