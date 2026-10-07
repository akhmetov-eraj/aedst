include(AedstCompilerWarnings)
include(AedstProtobuf)

# Wire types for the server's ConnectRPC services, generated from the single
# source of truth: apps/server/api/proto (+ the vendored protovalidate import,
# see third_party/protovalidate/README.md). Nothing in this module is
# hand-mirrored except the method tables (methods.hpp, streaming_methods.hpp), and
# their tests fail when they drift from the protos.
aedst_add_proto_library(AedstContractProto
    ROOTS
        "${AEDST_PROTO_ROOT}"
        "${PROJECT_SOURCE_DIR}/third_party/protovalidate")

add_library(AedstContract INTERFACE
    include/aedst/contract/methods.hpp
    include/aedst/contract/streaming_methods.hpp
    include/aedst/contract/timestamps.hpp)
add_library(Aedst::Contract ALIAS AedstContract)

target_include_directories(AedstContract INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(AedstContract INTERFACE AedstContractProto Aedst::Network)

# The server may not offer a notifications stream yet (docs/proposals/notifications-watch-rpc.md). The streaming
# table, the stream adapter and its tests exist only once the proto declares the RPC; no proto is edited here.
file(STRINGS "${AEDST_PROTO_ROOT}/aedst/notifications/v1/notifications.proto" aedst_watch_rpc
     REGEX "^[ \t]*rpc[ \t]+WatchNotifications[ \t]*\\(")
if(aedst_watch_rpc)
    target_compile_definitions(AedstContract INTERFACE AEDST_CONTRACT_HAS_WATCH_NOTIFICATIONS=1)
    set(AEDST_HAS_WATCH_NOTIFICATIONS ON CACHE INTERNAL "The server proto declares WatchNotifications")
    message(STATUS "AedstContract: the server declares WatchNotifications; notification streaming is available")
else()
    set(AEDST_HAS_WATCH_NOTIFICATIONS OFF CACHE INTERNAL "The server proto declares WatchNotifications")
    message(STATUS "AedstContract: no WatchNotifications RPC in the server protos; notifications are polled")
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
