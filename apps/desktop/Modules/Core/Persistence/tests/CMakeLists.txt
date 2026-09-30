find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(AedstPersistenceTests
    in_memory_master_key_provider.hpp
    temp_directory.hpp
    sqlite_connection_test.cpp
    secure_storage_test.cpp
    master_key_provider_test.cpp
    token_store_test.cpp
    migrator_test.cpp
)
target_link_libraries(AedstPersistenceTests PRIVATE Aedst::Persistence GTest::gtest_main)
aedst_set_warnings(AedstPersistenceTests)

gtest_discover_tests(AedstPersistenceTests
    TEST_PREFIX "AedstPersistence."
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS "unit")
