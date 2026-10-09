include(AedstCompilerWarnings)

find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Core)

# Capability negotiation (ADR-0035): the pure shown-iff-built-AND-server-enabled rules, the CapabilitiesService that
# fetches aedst.capabilities.v1 (public RPC), keeps the last answer in the SQLite `kv` table and implements the
# plugin module's ICapabilitySource, and the min/recommended version advice behind the forced-upgrade screen.
# Core module: knows no feature; the composition root supplies the client build (which features it contains).
add_library(AedstCapabilities STATIC
    include/aedst/capabilities/capabilities_service.hpp
    include/aedst/capabilities/capability_ports.hpp
    include/aedst/capabilities/capability_rules.hpp
    src/capabilities_service.cpp
    src/capability_adapters.cpp
    src/capability_rules.cpp
)
add_library(Aedst::Capabilities ALIAS AedstCapabilities)

target_include_directories(AedstCapabilities PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

# Contract: the generated messages and the method table. Update: semver parsing only.
target_link_libraries(AedstCapabilities
    PUBLIC Aedst::Core Aedst::Contract Aedst::Network Aedst::Persistence Aedst::Plugin Qt6::Core
    PRIVATE Aedst::Update Aedst::Logging)

aedst_set_warnings(AedstCapabilities)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
