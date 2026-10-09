find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(AedstPluginLoaderTests plugin_loader_test.cpp)
target_link_libraries(AedstPluginLoaderTests PRIVATE Aedst::PluginLoader GTest::gtest_main)
aedst_set_warnings(AedstPluginLoaderTests)

gtest_discover_tests(AedstPluginLoaderTests
    TEST_PREFIX "AedstPluginLoader."
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS "unit")
