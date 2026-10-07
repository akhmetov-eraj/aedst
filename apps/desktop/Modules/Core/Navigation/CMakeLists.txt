include(AedstCompilerWarnings)

find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Core)

# The desktop equivalent of Android's core/navigation (AedstNavGraph/AedstRoutes)
# and Apple's AedstNavigation (AedstRoute/Router): the only way one feature
# reaches another's screen.
add_library(AedstNavigation STATIC
    include/aedst/navigation/route.hpp
    include/aedst/navigation/nav_graph.hpp
    include/aedst/navigation/router.hpp
    src/router.cpp
)
add_library(Aedst::Navigation ALIAS AedstNavigation)

target_include_directories(AedstNavigation PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

# PUBLIC: Router is a QObject (QML drives the StackView from its signals) and
# NavGraph's inline code logs through AedstLogger.
target_link_libraries(AedstNavigation PUBLIC Aedst::Core Qt6::Core PRIVATE Aedst::Logging)

aedst_set_warnings(AedstNavigation)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
