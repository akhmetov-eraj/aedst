find_package(GTest REQUIRED)
find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Core Network)

# Shared test doubles (Apple's AedstTesting equivalent). Only added when
# BUILD_TESTING is ON, and only test targets may link it.
add_library(AedstTesting INTERFACE)
add_library(Aedst::Testing ALIAS AedstTesting)

target_sources(AedstTesting INTERFACE
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
        include/aedst/testing/fake_clock.hpp
        include/aedst/testing/behavior_fixture.hpp
        include/aedst/testing/fake_http_server.hpp
        include/aedst/testing/stub_token_provider.hpp
        include/aedst/testing/test_tls_material.hpp
        include/aedst/testing/test_main.hpp
)

target_link_libraries(AedstTesting INTERFACE
    Aedst::Core
    Aedst::Network
    Qt6::Core
    Qt6::Network
    GTest::gtest
)

# Cross-platform behavior fixtures (docs/contract/behavior/*.json): one repository file per behavior, replayed by the
# Web, Android, Apple and Desktop tests. Consumers read them through aedst/testing/behavior_fixture.hpp.
set(AEDST_BEHAVIOR_FIXTURE_DIR "${PROJECT_SOURCE_DIR}/../../docs/contract/behavior" CACHE PATH
    "Directory holding the shared behavior fixtures (docs/contract/behavior)")
target_compile_definitions(AedstTesting INTERFACE AEDST_BEHAVIOR_FIXTURE_DIR="${AEDST_BEHAVIOR_FIXTURE_DIR}")
