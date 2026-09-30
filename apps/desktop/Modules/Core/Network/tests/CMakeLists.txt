find_package(GTest REQUIRED)
include(GoogleTest)

# Pure logic (error model, envelope codec, retry schedule, interceptor): no event loop.
add_executable(AedstNetworkTests
    api_error_test.cpp
    call_options_test.cpp
    connect_envelope_test.cpp
    connect_error_test.cpp
)
target_link_libraries(AedstNetworkTests PRIVATE Aedst::Network Aedst::Testing GTest::gtest_main)
aedst_set_warnings(AedstNetworkTests)

gtest_discover_tests(AedstNetworkTests
    TEST_PREFIX "AedstNetwork."
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS "unit")

# ConnectClient against a loopback fake server: real QNetworkAccessManager + real
# Protobuf messages (Aedst::Contract), one QCoreApplication for the event loop.
# Non-discovering (DISCOVERY_MODE would launch the binary at build time, which
# needs the Qt DLLs on PATH on Windows); one CTest entry per binary like Qt Test.
add_executable(AedstNetworkClientTests connect_client_test.cpp)
target_link_libraries(AedstNetworkClientTests PRIVATE Aedst::Network Aedst::Contract Aedst::Testing)
aedst_set_warnings(AedstNetworkClientTests)

add_test(NAME AedstNetwork.ConnectClientTest COMMAND AedstNetworkClientTests)
set_tests_properties(AedstNetwork.ConnectClientTest PROPERTIES
    LABELS "unit"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
