find_package(GTest REQUIRED)
include(GoogleTest)

# The pure rules and the service against fakes, plus the shared fixture
# (docs/contract/behavior/capabilities.json) replayed through the real service (network and cache are doubles); a
# separate test round-trips the real SQLite cache on a DatabaseWorker.
add_executable(AedstCapabilitiesTests
    capabilities_replay_test.cpp
    capabilities_service_test.cpp
    capability_rules_test.cpp
    capability_test_support.hpp
)
target_link_libraries(AedstCapabilitiesTests PRIVATE Aedst::Capabilities Aedst::Testing GTest::gtest_main)
target_include_directories(AedstCapabilitiesTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
aedst_set_warnings(AedstCapabilitiesTests)

gtest_discover_tests(AedstCapabilitiesTests
    TEST_PREFIX "AedstCapabilities."
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS "unit")
