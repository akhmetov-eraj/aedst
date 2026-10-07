find_package(GTest REQUIRED)
find_package(OpenSSL REQUIRED)
include(GoogleTest)

# The test keypair is generated at run time with OpenSSL (no key material is
# committed). Aedst::Update's own OpenSSL/Qt dependencies are PRIVATE, so the
# test target links OpenSSL::Crypto itself.
add_executable(AedstUpdateTests
    artifact_verifier_test.cpp
    manifest_test.cpp
    manifest_verifier_test.cpp
    semver_test.cpp
    startup_health_guard_test.cpp
    update_checker_test.cpp
    url_validation_test.cpp
)
target_link_libraries(AedstUpdateTests PRIVATE Aedst::Update OpenSSL::Crypto GTest::gtest_main)
aedst_set_warnings(AedstUpdateTests)

gtest_discover_tests(AedstUpdateTests
    TEST_PREFIX "AedstUpdate."
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS "unit")
