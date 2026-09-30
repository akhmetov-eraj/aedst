include(AedstCompilerWarnings)

find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Core Network)
find_package(Protobuf CONFIG REQUIRED)

# The Connect-protocol client (see docs/adr/0001-api-contract-client.md). It is
# message-type agnostic: it moves serialized protobuf bytes and only needs
# libprotobuf's MessageLite. Concrete messages and the method table live in
# Aedst::Contract, which depends on this target - never the reverse.
add_library(AedstNetwork STATIC
    include/aedst/network/api_error.hpp
    include/aedst/network/asio_support.hpp
    include/aedst/network/auth_interceptor.hpp
    include/aedst/network/call_options.hpp
    include/aedst/network/connect_client.hpp
    include/aedst/network/connect_envelope.hpp
    include/aedst/network/connect_error.hpp
    include/aedst/network/method.hpp
    src/auth_interceptor.cpp
    src/connect_client.cpp
    src/connect_envelope.cpp
    src/connect_error.cpp
)
add_library(Aedst::Network ALIAS AedstNetwork)

target_include_directories(AedstNetwork PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

# PUBLIC: ConnectClient is a QObject and its templates use protobuf::MessageLite.
target_link_libraries(AedstNetwork PUBLIC Aedst::Core Qt6::Core Qt6::Network protobuf::libprotobuf)

if(WIN32)
    target_compile_definitions(AedstNetwork PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if(AEDST_ENABLE_ASIO)
    find_package(Boost REQUIRED)
    target_link_libraries(AedstNetwork PUBLIC Boost::headers)
    target_compile_definitions(AedstNetwork PUBLIC AEDST_WITH_BOOST_ASIO)
endif()

aedst_set_warnings(AedstNetwork)

if(BUILD_TESTING)
    add_subdirectory(tests)
    if(AEDST_BUILD_BENCHMARKS)
        add_subdirectory(bench)
    endif()
endif()
