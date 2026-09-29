cmake_minimum_required(VERSION 3.25)

project(Aedst
    VERSION 0.1.0
    DESCRIPTION "AEDST desktop client (Qt 6 / QML)"
    HOMEPAGE_URL "https://aedst.com"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(BUILD_TESTING "Build GoogleTest + Qt Test suites and register them with CTest" ON)
option(AEDST_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(AEDST_ENABLE_CLANG_TIDY "Run clang-tidy as part of the build (slow; CI runs it separately)" OFF)
option(AEDST_ENABLE_ASIO "Make Boost.Asio available to AedstNetwork (see asio_support.hpp)" OFF)
# Flatpak builds against org.kde.Platform, which already ships Qt — bundling
# Qt libraries into the install tree there would be wrong.
option(AEDST_DEPLOY_QT "Install Qt runtime libraries/plugins alongside the executable" ON)

# Qt is NOT a Conan dependency (see conanfile.py): it comes from the official
# Qt installer / aqtinstall, found through CMAKE_PREFIX_PATH (CMakePresets.json
# reads it from $QT_ROOT_DIR).
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick Network)
qt_standard_project_setup(REQUIRES 6.8)

include(GNUInstallDirs)
include(AedstCompilerWarnings)

if(AEDST_ENABLE_CLANG_TIDY)
    find_program(AEDST_CLANG_TIDY_EXE NAMES clang-tidy REQUIRED)
    set(CMAKE_CXX_CLANG_TIDY "${AEDST_CLANG_TIDY_EXE}")
endif()

if(BUILD_TESTING)
    enable_testing()
endif()

# Core: everything depends on these; they never depend on a feature.
add_subdirectory(Modules/Core/Common)
add_subdirectory(Modules/Core/Network)
add_subdirectory(Modules/Core/Persistence)
add_subdirectory(Modules/Core/Navigation)
add_subdirectory(Modules/Core/Analytics)
add_subdirectory(Modules/Core/DesignSystem)
if(BUILD_TESTING)
    add_subdirectory(Modules/Core/Testing)
endif()

# Features: depend on Core only, never on each other (see each feature's CMakeLists.txt).
add_subdirectory(Modules/Features/Auth)

# Composition root: the only target that links every feature. Also pulls in
# AedstPackaging.cmake (CPack) since packaging is about the executable.
add_subdirectory(App)

if(BUILD_TESTING)
    add_subdirectory(Tests)
endif()
