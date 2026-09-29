include(AedstCompilerWarnings)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Network)

add_library(AedstNetwork STATIC
    include/aedst/network/api_error.hpp
    include/aedst/network/api_client.hpp
    include/aedst/network/auth_interceptor.hpp
    include/aedst/network/endpoints.hpp
    include/aedst/network/asio_support.hpp
    include/aedst/network/dto/login_dto.hpp
    include/aedst/network/dto/user_dto.hpp
    include/aedst/network/dto/profile_dto.hpp
    src/api_client.cpp
    src/auth_interceptor.cpp
)
add_library(Aedst::Network ALIAS AedstNetwork)

target_include_directories(AedstNetwork PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

# PUBLIC: ApiClient is a QObject and the DTO helpers speak QJsonObject.
target_link_libraries(AedstNetwork PUBLIC Aedst::Core Qt6::Core Qt6::Network)

if(AEDST_ENABLE_ASIO)
    find_package(Boost REQUIRED)
    target_link_libraries(AedstNetwork PUBLIC Boost::headers)
    target_compile_definitions(AedstNetwork PUBLIC AEDST_WITH_BOOST_ASIO)
endif()

aedst_set_warnings(AedstNetwork)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
