find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(AedstCoreTests
    outcome_test.cpp
    logger_test.cpp
)
target_link_libraries(AedstCoreTests PRIVATE Aedst::Core GTest::gtest_main)
aedst_set_warnings(AedstCoreTests)

gtest_discover_tests(AedstCoreTests
    TEST_PREFIX "AedstCore."
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS "unit")
