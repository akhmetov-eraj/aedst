include(AedstCompilerWarnings)

# Feature plugin contract (ADR-0033): IFeaturePlugin, ServiceRegistry, PluginManager, capability
# gating and the pure trust decision for run-time plugins. Qt-free; a Core module that depends on
# Aedst::Core (and Aedst::Update for semver parsing only) and never on a feature.
add_library(AedstPlugin STATIC
    include/aedst/plugin/capability_set.hpp
    include/aedst/plugin/feature_plugin.hpp
    include/aedst/plugin/plugin_manager.hpp
    include/aedst/plugin/plugin_trust.hpp
    include/aedst/plugin/service_registry.hpp
    src/plugin_manager.cpp
    src/plugin_trust.cpp
)
add_library(Aedst::Plugin ALIAS AedstPlugin)

target_include_directories(AedstPlugin PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(AedstPlugin PUBLIC cxx_std_23)
target_link_libraries(AedstPlugin PUBLIC Aedst::Core PRIVATE Aedst::Update)

aedst_set_warnings(AedstPlugin)

# Optional run-time plugins: Qt plugin loading from a signed allowlist. Off by default, so a
# default build contains no code path that loads a library from disk (ADR-0033).
option(AEDST_ENABLE_RUNTIME_PLUGINS "Build the QPluginLoader-based loader for allowlisted, signed run-time plugins" OFF)
if(AEDST_ENABLE_RUNTIME_PLUGINS)
    add_subdirectory(loader)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
