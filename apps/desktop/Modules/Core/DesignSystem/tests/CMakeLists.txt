find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Test)

# Language selection rules (Qt-free header) + TranslationLoader's locale/layout side effects.
qt_add_executable(AedstDesignSystemQtTests translation_loader_test.cpp)
target_link_libraries(AedstDesignSystemQtTests PRIVATE Aedst::DesignSystem Qt6::Test)
aedst_set_warnings(AedstDesignSystemQtTests)

add_test(NAME AedstDesignSystem.TranslationLoaderTest COMMAND AedstDesignSystemQtTests)
set_tests_properties(AedstDesignSystem.TranslationLoaderTest PROPERTIES
    LABELS "qttest"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# QtQuickTest: every Aedst* component instantiates, plus the behaviour of the accessibility/keyboard/narrow-window changes
# (tst_designsystem.qml). Headless via the offscreen platform.
find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Qml Quick QuickTest)

qt_add_executable(AedstDesignSystemQmlTests qml_test_main.cpp)
target_link_libraries(AedstDesignSystemQmlTests PRIVATE
    Aedst::DesignSystem
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickTest)
target_compile_definitions(AedstDesignSystemQmlTests PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml")
aedst_set_warnings(AedstDesignSystemQmlTests)

add_test(NAME AedstDesignSystem.DesignSystemQmlTest COMMAND AedstDesignSystemQmlTests)
set_tests_properties(AedstDesignSystem.DesignSystemQmlTest PROPERTIES
    LABELS "qmltest"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_CONTROLS_STYLE=Basic")
