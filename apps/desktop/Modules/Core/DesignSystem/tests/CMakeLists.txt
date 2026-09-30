find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Test)

# Language selection rules (Qt-free header) + TranslationLoader's locale/layout side effects.
qt_add_executable(AedstDesignSystemQtTests translation_loader_test.cpp)
target_link_libraries(AedstDesignSystemQtTests PRIVATE Aedst::DesignSystem Qt6::Test)
aedst_set_warnings(AedstDesignSystemQtTests)

add_test(NAME AedstDesignSystem.TranslationLoaderTest COMMAND AedstDesignSystemQtTests)
set_tests_properties(AedstDesignSystem.TranslationLoaderTest PROPERTIES
    LABELS "qttest"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
