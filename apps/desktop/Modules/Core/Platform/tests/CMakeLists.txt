find_package(GTest REQUIRED)
find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Test)
include(GoogleTest)

# Pure logic + temp-dir file I/O (QSaveFile needs no event loop).
add_executable(AedstPlatformTests
    autostart_test.cpp
    deep_link_test.cpp
    platform_info_test.cpp
    toast_xml_test.cpp
)
target_link_libraries(AedstPlatformTests PRIVATE Aedst::Platform GTest::gtest_main)
aedst_set_warnings(AedstPlatformTests)

gtest_discover_tests(AedstPlatformTests
    TEST_PREFIX "AedstPlatform."
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS "unit")

# SingleInstanceGuard needs an event loop (QLocalServer): Qt Test, one CTest
# entry per binary.
qt_add_executable(AedstPlatformQtTests single_instance_guard_test.cpp)
target_link_libraries(AedstPlatformQtTests PRIVATE Aedst::Platform Qt6::Test)
aedst_set_warnings(AedstPlatformQtTests)

add_test(NAME AedstPlatform.SingleInstanceGuardTest COMMAND AedstPlatformQtTests)
set_tests_properties(AedstPlatform.SingleInstanceGuardTest PROPERTIES
    LABELS "qttest"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
