include(AedstCompilerWarnings)

find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Core)

# Run-time plugin loading (QPluginLoader) from a directory whose contents are pinned by a signed
# allowlist (ADR-0033). Only built with -DAEDST_ENABLE_RUNTIME_PLUGINS=ON; the default build has no
# code path that loads a library from disk.
add_library(AedstPluginLoader STATIC
    include/aedst/plugin/plugin_loader.hpp
    include/aedst/plugin/qt_feature_plugin.hpp
    src/plugin_loader.cpp
)
add_library(Aedst::PluginLoader ALIAS AedstPluginLoader)

target_include_directories(AedstPluginLoader PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

# PUBLIC Aedst::Update: the config carries update::TrustedKey (the same release keys that sign update manifests).
target_link_libraries(AedstPluginLoader
    PUBLIC
        Aedst::Plugin
        Aedst::Update
        Qt6::Core
    PRIVATE
        Aedst::Logging
)

aedst_set_warnings(AedstPluginLoader)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
