include(AedstCompilerWarnings)

# Widgets: QApplication + QSystemTrayIcon (native notifications); Multimedia: Aedst.Media players.
find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Widgets Multimedia)

qt_add_executable(aedst-desktop WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/app_dependencies.hpp
    src/app_dependencies.cpp
    src/session_info.hpp
    src/startup_trace.hpp
)

qt_add_qml_module(aedst-desktop
    URI Aedst.App
    VERSION 1.0
    QML_FILES qml/Main.qml
)

# main.cpp must answer `aedst-desktop --version` (prints AEDST_VERSION to stdout,
# exit 0, no display / QGuiApplication) - the CI install-verify job relies on it.
target_compile_definitions(aedst-desktop PRIVATE AEDST_VERSION="${PROJECT_VERSION}")

# Telemetry keys baked in at build time (root CMakeLists.txt cache variables; CI fills
# them from repository secrets). Environment variables of the same name override them.
# Empty = that integration stays off.
target_compile_definitions(aedst-desktop PRIVATE
    AEDST_SENTRY_DSN_DEFAULT="${AEDST_SENTRY_DSN}"
    AEDST_SENTRY_ENVIRONMENT_DEFAULT="${AEDST_SENTRY_ENVIRONMENT}"
    AEDST_POSTHOG_KEY_DEFAULT="${AEDST_POSTHOG_KEY}"
    AEDST_POSTHOG_HOST_DEFAULT="${AEDST_POSTHOG_HOST}")

# sentry-native's crashpad backend runs crash capture in a separate process,
# crashpad_handler(.exe), which must sit next to the executable (main.cpp passes
# that path). vcpkg installs it as a tool of the sentry-native port.
if(AEDST_ENABLE_SENTRY)
    find_program(AEDST_CRASHPAD_HANDLER
        NAMES crashpad_handler
        HINTS
            "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/tools/sentry-native"
            "${CMAKE_PREFIX_PATH}"
        PATH_SUFFIXES bin libexec tools/sentry-native
        NO_DEFAULT_PATH)
    if(AEDST_CRASHPAD_HANDLER)
        add_custom_command(TARGET aedst-desktop POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${AEDST_CRASHPAD_HANDLER}" "$<TARGET_FILE_DIR:aedst-desktop>"
            COMMENT "Copying crashpad_handler next to aedst-desktop"
            VERBATIM)
        if(APPLE)
            set(aedst_crashpad_destination "aedst-desktop.app/Contents/MacOS")
        else()
            set(aedst_crashpad_destination "${CMAKE_INSTALL_BINDIR}")
        endif()
        install(PROGRAMS "${AEDST_CRASHPAD_HANDLER}" DESTINATION "${aedst_crashpad_destination}")
    else()
        message(WARNING "AEDST: crashpad_handler not found; Sentry crash capture will not start "
                        "(sentry_init fails without it and the local CrashHandler is used instead).")
    endif()
endif()

# --- Translations -------------------------------------------------------------
# App-level strings (window titles, shell chrome). Module-owned strings are
# translated in their own modules. `cmake --build <preset> --target update_translations`
# refreshes the .ts sources; the .qm files are compiled and embedded (":/i18n")
# by `release_translations` as part of the normal build.
set(aedst_app_ts_files)
foreach(lang IN ITEMS en ru ar tg uz)
    set(ts "${CMAKE_CURRENT_SOURCE_DIR}/i18n/app_${lang}.ts")
    if(EXISTS "${ts}")
        list(APPEND aedst_app_ts_files "${ts}")
    else()
        message(WARNING "AEDST: missing translation source ${ts} (created by the UI work; skipped)")
    endif()
endforeach()
if(aedst_app_ts_files)
    qt_add_translations(aedst-desktop TS_FILES ${aedst_app_ts_files})
endif()

# --- Windows: version resource + icon --------------------------------------------
# Icon hook: drop a multi-resolution App/resources/icons/aedst.ico and it is embedded
# automatically (none is committed yet - see App/resources/icons/README.md).
if(WIN32)
    set(AEDST_VERSION "${PROJECT_VERSION}")
    set(AEDST_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
    set(AEDST_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
    set(AEDST_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
    set(aedst_ico "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/aedst.ico")
    if(EXISTS "${aedst_ico}")
        set(AEDST_WIN_ICON_LINE "IDI_ICON1 ICON \"${aedst_ico}\"")
    else()
        set(AEDST_WIN_ICON_LINE "// (no aedst.ico committed yet)")
    endif()
    configure_file("${PROJECT_SOURCE_DIR}/packaging/windows/aedst-desktop.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/aedst-desktop.rc" @ONLY)
    target_sources(aedst-desktop PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/aedst-desktop.rc")
endif()

# COMPOSITION ROOT: the only target in the tree that links every feature.
# It also links Core targets directly because AppDependencies constructs the
# shared infrastructure (ConnectClient, SQLite, TokenStore, Router, theme) that
# features receive by injection.
target_link_libraries(aedst-desktop PRIVATE
    Aedst::Feature::Auth
    Aedst::Feature::Tasks
    Aedst::Feature::Search
    Aedst::Feature::Notifications
    Aedst::Feature::Media
    Aedst::Feature::Admin
    Aedst::Core
    Aedst::Network
    Aedst::Persistence
    Aedst::Navigation
    Aedst::Analytics
    Aedst::DesignSystem
    Aedst::Connectivity
    Aedst::Platform
    Aedst::CrashReporting
    Aedst::Update
    Aedst::Telemetry
    Aedst::Media
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    Qt6::Multimedia
)

set_target_properties(aedst-desktop PROPERTIES
    OUTPUT_NAME "aedst-desktop"
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.aedst.Desktop"
    MACOSX_BUNDLE_BUNDLE_NAME "AEDST"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_COPYRIGHT "Copyright (C) AEDST"
)

# macOS bundle Info.plist (NSHighResolutionCapable, LSMinimumSystemVersion,
# NSSupportsAutomaticGraphicsSwitching). @AEDST_MACOS_MIN_VERSION@ is substituted by CMake.
# Icon hook: add App/resources/icons/aedst.icns and set MACOSX_BUNDLE_ICON_FILE + the
# file as a MACOSX_PACKAGE_LOCATION Resources source once it exists.
if(APPLE)
    set(AEDST_MACOS_MIN_VERSION "14.4")
    set(aedst_plist "${CMAKE_CURRENT_BINARY_DIR}/Info.plist.in")
    configure_file("${PROJECT_SOURCE_DIR}/packaging/macos/Info.plist.in" "${aedst_plist}" @ONLY)
    set_target_properties(aedst-desktop PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${aedst_plist}")
endif()

aedst_set_warnings(aedst-desktop)

# Release: split PDB / .debug / .dSYM into <build>/symbols (no-op for Debug).
aedst_split_debug_symbols(aedst-desktop)

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
