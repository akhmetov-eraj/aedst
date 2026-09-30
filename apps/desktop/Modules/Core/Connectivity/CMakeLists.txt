include(AedstCompilerWarnings)

find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Core Network)

# Reachability as QML/C++ state, on top of Qt's own QNetworkInformation (no
# hand-rolled OS probing). Core module: no dependency on any feature.
add_library(AedstConnectivity STATIC
    include/aedst/connectivity/reachability_source.hpp
    include/aedst/connectivity/connectivity_monitor.hpp
    src/connectivity_monitor.cpp
    src/qt_network_information_source.cpp
)
add_library(Aedst::Connectivity ALIAS AedstConnectivity)

target_include_directories(AedstConnectivity PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_link_libraries(AedstConnectivity PUBLIC Qt6::Core PRIVATE Qt6::Network)

aedst_set_warnings(AedstConnectivity)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
