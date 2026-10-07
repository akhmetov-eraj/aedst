find_package(GTest REQUIRED)
find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Test)
include(GoogleTest)

# Pure C++ (Domain) — GoogleTest + gMock.
add_executable(AedstFeatureAuthTests login_use_case_test.cpp)
target_link_libraries(AedstFeatureAuthTests PRIVATE Aedst::Feature::Auth GTest::gmock GTest::gtest_main)
aedst_set_warnings(AedstFeatureAuthTests)

gtest_discover_tests(AedstFeatureAuthTests
    TEST_PREFIX "AedstFeatureAuth."
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS "unit")

# Qt-dependent (Presentation) — Qt Test, one CTest entry per binary.
qt_add_executable(AedstFeatureAuthQtTests login_view_model_test.cpp)
target_link_libraries(AedstFeatureAuthQtTests PRIVATE Aedst::Feature::Auth Aedst::Analytics Qt6::Test)
aedst_set_warnings(AedstFeatureAuthQtTests)

add_test(NAME AedstFeatureAuth.LoginViewModelTest COMMAND AedstFeatureAuthQtTests)
set_tests_properties(AedstFeatureAuth.LoginViewModelTest PROPERTIES
    LABELS "qttest"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Data layer against a loopback fake server (real ConnectClient + Protobuf).
add_executable(AedstFeatureAuthDataTests connect_auth_remote_data_source_test.cpp)
target_link_libraries(AedstFeatureAuthDataTests PRIVATE Aedst::Feature::Auth Aedst::Testing)
aedst_set_warnings(AedstFeatureAuthDataTests)

add_test(NAME AedstFeatureAuth.ConnectAuthRemoteDataSourceTest COMMAND AedstFeatureAuthDataTests)
set_tests_properties(AedstFeatureAuth.ConnectAuthRemoteDataSourceTest PROPERTIES
    LABELS "unit"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# QtQuickTest UI tests: LoginView.qml (loaded from its qrc path) against fake
# LoginViewModel / Connectivity singletons. Runs headless via the offscreen platform.
find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Qml Quick QuickTest)

qt_add_executable(AedstFeatureAuthQmlTests qml_test_main.cpp)
target_link_libraries(AedstFeatureAuthQmlTests PRIVATE
    Aedst::Feature::Auth
    Aedst::DesignSystem
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickTest)
target_compile_definitions(AedstFeatureAuthQmlTests PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml")
aedst_set_warnings(AedstFeatureAuthQmlTests)

add_test(NAME AedstFeatureAuth.LoginViewQmlTest COMMAND AedstFeatureAuthQmlTests)
set_tests_properties(AedstFeatureAuth.LoginViewQmlTest PROPERTIES
    LABELS "qmltest"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_CONTROLS_STYLE=Basic")

# OidcAuthRepository against the scripted OpenID provider of the Aedst::Oidc tests (PKCE sign-in, refresh,
# logout through the real OidcSession; in-memory keychain, no browser).
add_executable(AedstFeatureAuthOidcTests oidc_auth_repository_test.cpp)
target_link_libraries(AedstFeatureAuthOidcTests PRIVATE Aedst::Feature::Auth Aedst::Oidc Aedst::Testing Qt6::Network GTest::gtest)
target_include_directories(AedstFeatureAuthOidcTests PRIVATE ${PROJECT_SOURCE_DIR}/Modules/Core/Oidc/tests)
aedst_set_warnings(AedstFeatureAuthOidcTests)

add_test(NAME AedstFeatureAuth.OidcAuthRepositoryTest COMMAND AedstFeatureAuthOidcTests)
set_tests_properties(AedstFeatureAuth.OidcAuthRepositoryTest PROPERTIES
    LABELS "unit"
    TIMEOUT 120
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# The first-party session store: reads from memory, writes through the database thread (no Qt event loop needed).
add_executable(AedstFeatureAuthStoreTests local_token_store_adapter_test.cpp)
target_link_libraries(AedstFeatureAuthStoreTests PRIVATE Aedst::Feature::Auth Aedst::Persistence GTest::gtest_main)
aedst_set_warnings(AedstFeatureAuthStoreTests)

add_test(NAME AedstFeatureAuth.LocalTokenStoreAdapterTest COMMAND AedstFeatureAuthStoreTests)
set_tests_properties(AedstFeatureAuth.LocalTokenStoreAdapterTest PROPERTIES LABELS "unit" TIMEOUT 120)
