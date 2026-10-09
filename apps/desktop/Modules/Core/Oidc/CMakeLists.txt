include(AedstCompilerWarnings)
include(AedstQtKeychain)

# QtNetworkAuth is an add-on module of the Qt installer ("Qt Network Authorization").
find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Core Gui Network NetworkAuth)

aedst_fetch_qtkeychain()

# OpenID Connect client for the desktop app (ZITADEL): Authorization Code + PKCE S256 in the
# system browser (QtNetworkAuth), refresh token in the OS keychain (QtKeychain), single-flight
# refresh, logout with revocation and end_session. See docs/adr/0025-oidc-authentication.md.
# A Core module: it depends on Aedst::Core and Aedst::Network (TokenProvider), never on a feature.
add_library(AedstOidc STATIC
    include/aedst/oidc/id_token_claims.hpp
    include/aedst/oidc/oidc_config.hpp
    include/aedst/oidc/oidc_discovery.hpp
    include/aedst/oidc/oidc_session.hpp
    include/aedst/oidc/refresh_token_store.hpp
    src/id_token_claims.cpp
    src/keychain_refresh_token_store.cpp
    src/oidc_config.cpp
    src/oidc_discovery.cpp
    src/oidc_session.cpp
    src/oidc_session_internal.hpp
    src/oidc_session_signin.cpp
    src/oidc_session_signout.cpp
    src/refresh_token_store.cpp
)
add_library(Aedst::Oidc ALIAS AedstOidc)

target_include_directories(AedstOidc PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

# PUBLIC: OidcSession is a QObject and a network::TokenProvider; its API speaks core::Outcome.
# QtNetworkAuth, QtKeychain and QtGui (QDesktopServices) stay implementation details.
target_link_libraries(AedstOidc
    PUBLIC
        Aedst::Core
        Aedst::Network
        Qt6::Core
        Qt6::Network
    PRIVATE
        Aedst::Logging
        Qt6::Gui
        Qt6::NetworkAuth
        ${AEDST_QTKEYCHAIN_TARGET}
)
target_include_directories(AedstOidc PRIVATE ${AEDST_QTKEYCHAIN_INCLUDE_DIRS})

aedst_set_warnings(AedstOidc)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
