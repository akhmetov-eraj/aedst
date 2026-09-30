include(AedstCompilerWarnings)

find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)

qt_add_executable(aedst-desktop WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/app_dependencies.hpp
    src/app_dependencies.cpp
    src/startup_trace.hpp
)

qt_add_qml_module(aedst-desktop
    URI Aedst.App
    VERSION 1.0
    QML_FILES qml/Main.qml
)

target_compile_definitions(aedst-desktop PRIVATE AEDST_VERSION="${PROJECT_VERSION}")

# COMPOSITION ROOT: the only target in the tree that links every feature.
# It also links Core targets directly because AppDependencies constructs the
# shared infrastructure (ConnectClient, SQLite, TokenStore, Router, theme) that
# features receive by injection.
target_link_libraries(aedst-desktop PRIVATE
    Aedst::Feature::Auth
    Aedst::Core
    Aedst::Network
    Aedst::Persistence
    Aedst::Navigation
    Aedst::Analytics
    Aedst::DesignSystem
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

set_target_properties(aedst-desktop PROPERTIES
    OUTPUT_NAME "aedst-desktop"
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.aedst.Desktop"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
)

aedst_set_warnings(aedst-desktop)

install(TARGETS aedst-desktop
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(AEDST_DEPLOY_QT)
    # windeployqt / macdeployqt / Qt's Linux deploy, driven by the QML imports it finds.
    qt_generate_deploy_qml_app_script(
        TARGET aedst-desktop
        OUTPUT_SCRIPT aedst_deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${aedst_deploy_script})
endif()

include(AedstPackaging)
