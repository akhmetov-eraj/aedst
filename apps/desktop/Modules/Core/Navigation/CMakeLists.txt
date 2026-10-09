include(AedstCompilerWarnings)

find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Core)

# The desktop equivalent of Android's core/navigation (AedstNavGraph/AedstRoutes)
# and Apple's AedstNavigation (AedstRoute/Router): the only way one feature
# reaches another's screen.
add_library(AedstNavigation STATIC
    include/aedst/navigation/route.hpp
    include/aedst/navigation/route_names.hpp
    include/aedst/navigation/nav_graph.hpp
    include/aedst/navigation/router.hpp
    include/aedst/navigation/deep_link_gate.hpp
    src/router.cpp
)
add_library(Aedst::Navigation ALIAS AedstNavigation)

# Declarative QML singletons (QML_ELEMENT + QML_SINGLETON, Aedst::QmlSupport); the module URI is the import name QML uses.
# Only App links AedstNavigationplugin, so QML test executables keep registering their fakes under this URI.
target_link_libraries(AedstNavigation PUBLIC Aedst::QmlSupport)
qt_add_qml_module(AedstNavigation URI Aedst.Navigation VERSION 1.0 RESOURCE_PREFIX /qt/qml)

target_include_directories(AedstNavigation PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

# PUBLIC: Router is a QObject (QML drives the StackView from its signals) and
# NavGraph's inline code logs through the Qt category lcNavigation (public header, so Logging is PUBLIC).
target_link_libraries(AedstNavigation PUBLIC Aedst::Core Aedst::Logging Qt6::Core)

aedst_set_warnings(AedstNavigation)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
