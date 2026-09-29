include(AedstCompilerWarnings)

add_library(AedstAnalytics STATIC
    include/aedst/analytics/analytics_tracker.hpp
    include/aedst/analytics/no_op_analytics_tracker.hpp
    src/no_op_analytics_tracker.cpp
)
add_library(Aedst::Analytics ALIAS AedstAnalytics)

target_include_directories(AedstAnalytics PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(AedstAnalytics PUBLIC cxx_std_23)

# PRIVATE: only the .cpp logs; the interface is plain std types.
target_link_libraries(AedstAnalytics PRIVATE Aedst::Core)

aedst_set_warnings(AedstAnalytics)
