find_package(GTest REQUIRED)
include(GoogleTest)

# Domain (use case validation, overdue rule) + Data mapping (proto -> Domain).
add_executable(AedstFeatureTasksTests create_task_use_case_test.cpp)
target_link_libraries(AedstFeatureTasksTests PRIVATE Aedst::Feature::Tasks GTest::gmock GTest::gtest_main)
aedst_set_warnings(AedstFeatureTasksTests)

add_test(NAME AedstFeatureTasks.UnitTests COMMAND AedstFeatureTasksTests)
set_tests_properties(AedstFeatureTasks.UnitTests PROPERTIES LABELS "unit")
