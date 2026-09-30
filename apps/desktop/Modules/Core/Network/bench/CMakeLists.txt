find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Test)

# Qt Test benchmarks (QBENCHMARK) for the per-call hot path: envelope framing,
# Protobuf (de)serialization of the auth messages, Connect error-body parsing.
# Built only with -DAEDST_BUILD_BENCHMARKS=ON (the `bench` preset) and registered
# under the CTest label `bench`, so a normal `ctest` never runs them. Numbers
# are only meaningful in a Release build (the preset already is one).
qt_add_executable(AedstNetworkBench network_bench.cpp)
target_link_libraries(AedstNetworkBench PRIVATE Aedst::Network Aedst::Contract Qt6::Test)
aedst_set_warnings(AedstNetworkBench)

add_test(NAME AedstNetwork.Bench COMMAND AedstNetworkBench)
set_tests_properties(AedstNetwork.Bench PROPERTIES LABELS "bench")
