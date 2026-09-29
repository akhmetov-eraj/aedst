find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(AedstPersistenceTests sqlite_connection_test.cpp)
target_link_libraries(AedstPersistenceTests PRIVATE Aedst::Persistence GTest::gtest_main)
aedst_set_warnings(AedstPersistenceTests)

gtest_discover_tests(AedstPersistenceTests
    TEST_PREFIX "AedstPersistence."
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS "unit")
