include(AedstCompilerWarnings)
include(AedstProtobuf)

# Wire types for the server's ConnectRPC services, generated from the single
# source of truth: apps/server/api/proto (+ the vendored protovalidate import,
# see third_party/protovalidate/README.md). Nothing in this module is
# hand-mirrored except the method table (methods.hpp), and its test fails when
# it drifts from the protos.
aedst_add_proto_library(AedstContractProto
    ROOTS
        "${AEDST_PROTO_ROOT}"
        "${PROJECT_SOURCE_DIR}/third_party/protovalidate")

add_library(AedstContract INTERFACE
    include/aedst/contract/methods.hpp
    include/aedst/contract/timestamps.hpp)
add_library(Aedst::Contract ALIAS AedstContract)

target_include_directories(AedstContract INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(AedstContract INTERFACE AedstContractProto Aedst::Network)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
