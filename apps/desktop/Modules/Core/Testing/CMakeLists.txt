find_package(GTest REQUIRED)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core)

# Shared test doubles (Apple's AedstTesting equivalent). Only added when
# BUILD_TESTING is ON, and only test targets may link it.
add_library(AedstTesting INTERFACE)
add_library(Aedst::Testing ALIAS AedstTesting)

target_sources(AedstTesting INTERFACE
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
        include/aedst/testing/fake_clock.hpp
        include/aedst/testing/stub_token_provider.hpp
        include/aedst/testing/test_main.hpp
)

target_link_libraries(AedstTesting INTERFACE
    Aedst::Core
    Aedst::Network
    Qt6::Core
    GTest::gtest
)
