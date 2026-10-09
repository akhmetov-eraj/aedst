include_guard(GLOBAL)

# aedst_add_proto_library(<target> ROOTS <proto-root>...)
#
# Builds a STATIC library of protoc-generated C++ (messages only, no services -
# the Connect wire protocol lives in Aedst::Network) for every *.proto found under
# each root. A proto's path relative to its root is its import name, so generated
# headers are included as <aedst/auth/v1/auth.pb.h>, matching `import` paths.
#
# Why protoc-at-build-time (and not `buf generate`, nor committed output):
#  - Generated C++ is only valid against the libprotobuf it was generated for.
#    Using the protoc that ships with the linked package makes version skew
#    impossible; committed or buf-produced output would need a lockstep bump.
#  - `buf generate`'s C++ path (`protoc_builtin: cpp`) shells out to protoc anyway
#    and needs BSR network access for buf.build/bufbuild/protovalidate. Its
#    imports are vendored in third_party/protovalidate instead (see its README).
# See docs/adr/0001-api-contract-client.md.
function(aedst_add_proto_library target)
    cmake_parse_arguments(ARG "" "" "ROOTS" ${ARGN})
    if(NOT ARG_ROOTS)
        message(FATAL_ERROR "aedst_add_proto_library(${target}): ROOTS is required")
    endif()

    find_package(Protobuf CONFIG REQUIRED)

    set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
    set(include_flags "")
    set(proto_files "")
    set(proto_names "")
    set(generated_sources "")
    set(generated_headers "")

    foreach(root IN LISTS ARG_ROOTS)
        if(NOT IS_DIRECTORY "${root}")
            message(FATAL_ERROR "aedst_add_proto_library(${target}): proto root '${root}' does not exist. "
                                "For the server contract set -DAEDST_PROTO_ROOT=<path to apps/server/api/proto>.")
        endif()
        list(APPEND include_flags "-I${root}")
        file(GLOB_RECURSE root_protos CONFIGURE_DEPENDS "${root}/*.proto")
        foreach(proto IN LISTS root_protos)
            file(RELATIVE_PATH relative "${root}" "${proto}")
            string(REGEX REPLACE "\\.proto$" "" stem "${relative}")
            list(APPEND proto_files "${proto}")
            list(APPEND proto_names "${relative}")
            list(APPEND generated_sources "${generated_dir}/${stem}.pb.cc")
            list(APPEND generated_headers "${generated_dir}/${stem}.pb.h")
        endforeach()
    endforeach()

    if(NOT proto_files)
        message(FATAL_ERROR "aedst_add_proto_library(${target}): no .proto files under ${ARG_ROOTS}")
    endif()

    # protoc must also see google/protobuf/*.proto (timestamp, field_mask, descriptor...).
    # vcpkg installs them under the package's include dir, which is not always
    # next to the protoc binary, so pass it explicitly.
    get_target_property(protobuf_include_dirs protobuf::libprotobuf INTERFACE_INCLUDE_DIRECTORIES)
    foreach(dir IN LISTS protobuf_include_dirs)
        if(EXISTS "${dir}/google/protobuf/timestamp.proto")
            list(APPEND include_flags "-I${dir}")
            break()
        endif()
    endforeach()

    add_custom_command(
        OUTPUT ${generated_sources} ${generated_headers}
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${generated_dir}"
        COMMAND protobuf::protoc "--cpp_out=${generated_dir}" ${include_flags} ${proto_files}
        DEPENDS ${proto_files} protobuf::protoc
        COMMENT "protoc: generating C++ for ${target} (${CMAKE_CURRENT_BINARY_DIR}/generated)"
        VERBATIM)

    add_library(${target} STATIC ${generated_sources} ${generated_headers})
    aedst_set_hardening(${target}) # generated code is first-party in the shipped binary: same exploit mitigations
    target_include_directories(${target} SYSTEM PUBLIC "$<BUILD_INTERFACE:${generated_dir}>")
    target_link_libraries(${target} PUBLIC protobuf::libprotobuf)

    set_target_properties(${target} PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
        # Generated code is third-party for lint purposes.
        CXX_CLANG_TIDY ""
        POSITION_INDEPENDENT_CODE ON)

    # Import names ("aedst/auth/v1/auth.proto"), for tests that walk the generated descriptors.
    set(${target}_PROTO_NAMES "${proto_names}" PARENT_SCOPE)

    if(WIN32)
        # <windows.h> min/max would break absl/protobuf headers.
        target_compile_definitions(${target} PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
    endif()
    if(MSVC)
        # Silence warnings from generated code and (via /external:I from SYSTEM) its headers.
        target_compile_options(${target} PRIVATE /W1 /external:W0)
        target_compile_options(${target} INTERFACE /external:W0)
    endif()
endfunction()
