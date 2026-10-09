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

# Declarative QML singletons (QML_ELEMENT + QML_SINGLETON, Aedst::QmlSupport); the module URI is the import name QML uses.
# Only App links AedstConnectivityplugin, so QML test executables keep registering their fakes under this URI.
target_link_libraries(AedstConnectivity PUBLIC Aedst::QmlSupport)
qt_add_qml_module(AedstConnectivity URI Aedst.Connectivity VERSION 1.0 RESOURCE_PREFIX /qt/qml)

target_include_directories(AedstConnectivity PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_link_libraries(AedstConnectivity PUBLIC Qt6::Core PRIVATE Qt6::Network)

aedst_set_warnings(AedstConnectivity)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
