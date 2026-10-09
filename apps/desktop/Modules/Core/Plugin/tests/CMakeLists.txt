find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(AedstPluginTests
    plugin_manager_test.cpp
    plugin_trust_test.cpp
    service_registry_test.cpp
)
target_link_libraries(AedstPluginTests PRIVATE Aedst::Plugin GTest::gtest_main)
aedst_set_warnings(AedstPluginTests)

gtest_discover_tests(AedstPluginTests
    TEST_PREFIX "AedstPlugin."
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS "unit")
