find_package(GTest REQUIRED)
include(GoogleTest)

# Uses AEDST_GTEST_MAIN_WITH_QCOREAPPLICATION (Aedst::Testing) instead of
# gtest_main because Router is a QObject.
add_executable(AedstNavigationTests router_test.cpp)
target_link_libraries(AedstNavigationTests PRIVATE Aedst::Navigation Aedst::Testing GTest::gtest)
aedst_set_warnings(AedstNavigationTests)

gtest_discover_tests(AedstNavigationTests
    TEST_PREFIX "AedstNavigation."
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS "unit")
