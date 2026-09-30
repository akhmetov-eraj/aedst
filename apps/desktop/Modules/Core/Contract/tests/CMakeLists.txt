find_package(GTest REQUIRED)
include(GoogleTest)

# Descriptor-drift and wire-format tests. The list of proto files under the
# server's proto root is handed to the test so a *new* service cannot slip in
# without a methods.hpp entry.
string(REPLACE ";" "|" aedst_proto_names "${AedstContractProto_PROTO_NAMES}")

add_executable(AedstContractTests
    methods_test.cpp
    wire_format_test.cpp
)
target_link_libraries(AedstContractTests PRIVATE Aedst::Contract GTest::gtest_main)
target_compile_definitions(AedstContractTests PRIVATE AEDST_CONTRACT_PROTO_NAMES="${aedst_proto_names}")
aedst_set_warnings(AedstContractTests)

gtest_discover_tests(AedstContractTests
    TEST_PREFIX "AedstContract."
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS "unit")
