cmake_minimum_required(VERSION 3.25)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Must run before project(): sets the compiler launcher (sccache/ccache).
include(AedstToolchainHints)

# macOS: the deployment target must be known before project() (it feeds the
# compiler/SDK probe). The macos-* presets set it explicitly; this is the fallback
# for a plain `cmake -S . -B build` on a Mac. Keep in sync with LSMinimumSystemVersion
# (App/CMakeLists.txt) and the macOS 12 minimum in docs/content/library-first.md.
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
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
option(AEDST_ENABLE_ASIO "Make Boost.Asio available to AedstNetwork (see asio_support.hpp)" OFF)
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
set(aedst_qt_components Core Gui Qml Quick QuickControls2 Network LinguistTools)
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
add_subdirectory(Modules/Core/Platform)        # Aedst::Platform (secure storage, notifications, autostart)
add_subdirectory(Modules/Core/Network)
add_subdirectory(Modules/Core/Contract)
add_subdirectory(Modules/Core/Connectivity)    # Aedst::Connectivity
add_subdirectory(Modules/Core/CrashReporting)  # Aedst::CrashReporting
add_subdirectory(Modules/Core/Update)          # Aedst::Update
add_subdirectory(Modules/Core/Persistence)
add_subdirectory(Modules/Core/Navigation)
add_subdirectory(Modules/Core/Analytics)
add_subdirectory(Modules/Core/DesignSystem)

# Features: depend on Core only, never on each other (see each feature's CMakeLists.txt).
add_subdirectory(Modules/Features/Auth)

# Composition root: the only target that links every feature. Also pulls in
# AedstPackaging.cmake (CPack) since packaging is about the executable.
add_subdirectory(App)

if(BUILD_TESTING)
    add_subdirectory(Tests)
endif()
