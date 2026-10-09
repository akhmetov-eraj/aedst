find_package(GTest REQUIRED)

# Real sockets and QNetworkAccessManager against a scripted loopback OpenID provider
# (oidc_test_support.hpp): discovery, authorization redirect, PKCE S256 verification at the token
# endpoint, refresh with rotation, revocation. No browser, no keychain (InMemoryRefreshTokenStore),
# no external network. One QCoreApplication for the whole binary.
add_executable(AedstOidcTests
    id_token_claims_test.cpp
    oidc_config_test.cpp
    oidc_discovery_test.cpp
    oidc_session_test.cpp
    oidc_test_main.cpp
    refresh_token_store_test.cpp
    session_refresh_replay_test.cpp
)
target_link_libraries(AedstOidcTests PRIVATE Aedst::Oidc Aedst::Contract Aedst::Testing Qt6::Network GTest::gtest)
target_include_directories(AedstOidcTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
aedst_set_warnings(AedstOidcTests)

add_test(NAME AedstOidc.OidcTest COMMAND AedstOidcTests)
set_tests_properties(AedstOidc.OidcTest PROPERTIES
    LABELS "unit"
    TIMEOUT 180
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
