find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Qml Quick QuickControls2 QuickTest)

# QtQuickTest of the app shell: App/qml/Main.qml is loaded from source against fakes of the singletons it imports
# (tst_main_shell.qml: 480 x 480 layout, hidden features, the error banner's Escape handling, the offline footer).
# The executable aedst-desktop cannot be linked into a test, so only what the QML needs is: the design system, the real
# Routes singleton class (Aedst::Navigation) and the fakes in app_qml_test_main.cpp.
qt_add_executable(AedstAppQmlTests app_qml_test_main.cpp)
target_link_libraries(AedstAppQmlTests PRIVATE
    Aedst::DesignSystem
    Aedst::Navigation
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickTest)
target_compile_definitions(AedstAppQmlTests PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml"
    AEDST_APP_TEST_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml"
    AEDST_APP_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../qml")
aedst_set_warnings(AedstAppQmlTests)

add_test(NAME AedstApp.MainShellQmlTest COMMAND AedstAppQmlTests)
set_tests_properties(AedstApp.MainShellQmlTest PROPERTIES
    LABELS "qmltest"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_CONTROLS_STYLE=Basic")
