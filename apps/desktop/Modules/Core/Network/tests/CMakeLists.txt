find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(AedstNetworkTests api_error_test.cpp)
target_link_libraries(AedstNetworkTests PRIVATE Aedst::Network GTest::gtest_main)
aedst_set_warnings(AedstNetworkTests)

gtest_discover_tests(AedstNetworkTests
    TEST_PREFIX "AedstNetwork."
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS "unit")
