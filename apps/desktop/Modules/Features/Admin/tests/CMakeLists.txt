find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Test)

# The ViewModel against a scripted repository (Qt Test: needs an event loop for the debounce timer) and the rows model.
qt_add_executable(AedstFeatureAdminTests admin_view_model_test.cpp)
target_link_libraries(AedstFeatureAdminTests PRIVATE Aedst::Feature::Admin Aedst::Analytics Qt6::Test)
aedst_set_warnings(AedstFeatureAdminTests)

add_test(NAME AedstFeatureAdmin.AdminViewModelTest COMMAND AedstFeatureAdminTests)
set_tests_properties(AedstFeatureAdmin.AdminViewModelTest PROPERTIES
    LABELS "qttest"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# QtQuickTest of AdminView.qml (loaded from source) against the real AdminViewModel and a scripted repository.
find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Qml Quick QuickControls2 QuickTest)

qt_add_executable(AedstFeatureAdminQmlTests qml_test_main.cpp)
target_link_libraries(AedstFeatureAdminQmlTests PRIVATE
    Aedst::Feature::Admin
    Aedst::Analytics
    Aedst::DesignSystem
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickTest)
target_compile_definitions(AedstFeatureAdminQmlTests PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml"
    AEDST_ADMIN_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../Presentation")
target_include_directories(AedstFeatureAdminQmlTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
aedst_set_warnings(AedstFeatureAdminQmlTests)

add_test(NAME AedstFeatureAdmin.AdminViewQmlTest COMMAND AedstFeatureAdminQmlTests)
set_tests_properties(AedstFeatureAdmin.AdminViewQmlTest PROPERTIES
    LABELS "qmltest"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_CONTROLS_STYLE=Basic")
