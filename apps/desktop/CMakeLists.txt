cmake_minimum_required(VERSION 3.25)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Must run before project(): sets the compiler launcher (sccache/ccache).
include(AedstToolchainHints)

# Desktop targets Windows and Linux only; macOS is served by apps/apple (ADR-0023).
if(APPLE)
    message(FATAL_ERROR "AEDST Desktop supports Windows and Linux only. macOS is served by the native app in apps/apple.")
endif()

project(Aedst
    VERSION 0.1.0
    DESCRIPTION "AEDST desktop client (Qt 6 / QML)"
    HOMEPAGE_URL "https://aedst.com"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Windows version resource: App/CMakeLists.txt adds a generated .rc to the executable.
if(WIN32)
    enable_language(RC)
endif()

option(BUILD_TESTING "Build GoogleTest + Qt Test suites and register them with CTest" ON)
option(AEDST_BUILD_BENCHMARKS "Build the Qt Test (QBENCHMARK) benchmarks; requires BUILD_TESTING" OFF)
option(AEDST_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(AEDST_ENABLE_CLANG_TIDY "Run clang-tidy as part of the build (slow; CI runs it separately)" OFF)
option(AEDST_ENABLE_SENTRY "Link sentry-native (crashpad backend) into Aedst::Telemetry; inert at runtime without a DSN" ON)
# Build-time telemetry keys. The environment variables of the same name override them at
# runtime. Empty = that integration is disabled. Neither is a secret in the cryptographic
# sense (both end up inside the binary), but keep them out of the repo: CI passes them
# from repository secrets.
set(AEDST_SENTRY_DSN "" CACHE STRING "Sentry DSN baked into the build (env AEDST_SENTRY_DSN overrides)")
set(AEDST_SENTRY_ENVIRONMENT "production" CACHE STRING "Sentry environment tag")
set(AEDST_POSTHOG_KEY "" CACHE STRING "PostHog project API key (env AEDST_POSTHOG_KEY overrides)")
set(AEDST_POSTHOG_HOST "https://us.i.posthog.com" CACHE STRING "PostHog ingestion host, https only (env AEDST_POSTHOG_HOST overrides)")
# Self-update sources (docs/adr/0029-delivery-updates-signing.md). Environment variables of the same name override them.
# Empty manifest URL, or a build without AEDST_UPDATE_PUBLIC_KEYS = updates off.
set(AEDST_UPDATE_MANIFEST_URL "" CACHE STRING "https URL of the signed update manifest (env AEDST_UPDATE_MANIFEST_URL overrides)")
set(AEDST_UPDATE_FEED_URL "" CACHE STRING "Base https URL of the Velopack release feed (env AEDST_UPDATE_FEED_URL overrides)")
set(AEDST_UPDATE_CHANNEL "stable" CACHE STRING "Update channel: stable or beta (env AEDST_UPDATE_CHANNEL overrides)")
# Flatpak builds against org.kde.Platform, which already ships Qt — bundling
# Qt libraries into the install tree there would be wrong.
option(AEDST_DEPLOY_QT "Install Qt runtime libraries/plugins alongside the executable" ON)

# The server's proto tree is the single source of truth for the wire contract.
# Generated C++ is produced at build time by protoc (see cmake/AedstProtobuf.cmake).
set(AEDST_PROTO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../server/api/proto" CACHE PATH
    "Directory holding the server's .proto files (apps/server/api/proto)")

include(GNUInstallDirs)
include(AedstCompilerFeatures)
include(AedstBuildOptions)
# Sanitizer flags are global compile/link options: they must be registered before
# any target is created (every add_subdirectory below), so this include stays here.
include(AedstSanitizers)
include(AedstQtVersion)
include(AedstCompilerWarnings)

# Qt is NOT a package-manager dependency (see vcpkg.json): it comes from the
# official binaries (Qt installer / aqtinstall) at the version pinned in
# qt-version.txt, found through CMAKE_PREFIX_PATH (the presets read $QT_ROOT_DIR).
#
# LinguistTools: lupdate/lrelease for qt_add_translations. QuickControls2: design
# system + app. Test / QuickTest: Qt Test and QtQuickTest suites (BUILD_TESTING
# only). DBus is optional (Linux secret service / notifications / autostart in
# AedstPlatform); modules test `TARGET Qt6::DBus` before using it.
# Multimedia: Aedst::Media (MediaPlayer/VideoOutput, QVideoSink frame grabs) - an
# add-on module: `aqt install-qt ... -m qtmultimedia` (CI passes `modules: qtmultimedia`).
# Widgets: QApplication + QSystemTrayIcon for native desktop notifications
# (docs/adr/0022-telemetry-media-notifications.md); no Qt Widgets UI is built.
set(aedst_qt_components Core Gui Qml Quick QuickControls2 Network Multimedia Widgets LinguistTools)
if(BUILD_TESTING)
    list(APPEND aedst_qt_components Test QuickTest)
endif()
find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED
    COMPONENTS ${aedst_qt_components}
    OPTIONAL_COMPONENTS DBus)
aedst_check_qt_version()
qt_standard_project_setup(REQUIRES ${AEDST_QT_MINIMUM})

if(AEDST_ENABLE_CLANG_TIDY)
    find_program(AEDST_CLANG_TIDY_EXE NAMES clang-tidy REQUIRED)
    set(CMAKE_CXX_CLANG_TIDY "${AEDST_CLANG_TIDY_EXE}")
endif()

if(BUILD_TESTING)
    enable_testing()
endif()

# Shared test doubles first: the Core modules' tests/ subdirectories link Aedst::Testing.
if(BUILD_TESTING)
    add_subdirectory(Modules/Core/Testing)
endif()

# Core: everything depends on these; they never depend on a feature.
add_subdirectory(Modules/Core/Common)
add_subdirectory(Modules/Core/Logging)         # Aedst::Logging (Qt logging categories)
add_subdirectory(Modules/Core/Platform)        # Aedst::Platform (secure storage, notifications, autostart)
add_subdirectory(Modules/Core/Network)
add_subdirectory(Modules/Core/Oidc)            # Aedst::Oidc (OIDC + PKCE sign-in, QtKeychain refresh token)
add_subdirectory(Modules/Core/Contract)
add_subdirectory(Modules/Core/Connectivity)    # Aedst::Connectivity
add_subdirectory(Modules/Core/CrashReporting)  # Aedst::CrashReporting
add_subdirectory(Modules/Core/Update)          # Aedst::Update
add_subdirectory(Modules/Core/Updater)         # Aedst::Updater (self-update orchestration, Velopack adapter)
add_subdirectory(Modules/Core/Persistence)
add_subdirectory(Modules/Core/Navigation)
add_subdirectory(Modules/Core/Analytics)
add_subdirectory(Modules/Core/Telemetry)       # Aedst::Telemetry (Sentry, PostHog, Qt log bridge, ErrorCenter)
add_subdirectory(Modules/Core/DesignSystem)
add_subdirectory(Modules/Core/Media)           # Aedst::Media (upload, image cache, video, thumbnails, compression)

# Features: depend on Core only, never on each other (see each feature's CMakeLists.txt).
add_subdirectory(Modules/Features/Auth)
add_subdirectory(Modules/Features/Tasks)
add_subdirectory(Modules/Features/Search)
add_subdirectory(Modules/Features/Notifications)
add_subdirectory(Modules/Features/Media)
add_subdirectory(Modules/Features/Admin)

# Composition root: the only target that links every feature. Also pulls in
# AedstPackaging.cmake (CPack) since packaging is about the executable.
add_subdirectory(App)

if(BUILD_TESTING)
    add_subdirectory(Tests)
endif()
