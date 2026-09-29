find_package(GTest REQUIRED)
find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
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
