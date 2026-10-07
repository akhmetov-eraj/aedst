find_package(GTest REQUIRED)

# Pure Domain: which polled notifications become native desktop notifications.
add_executable(AedstFeatureNotificationsTests new_notification_detector_test.cpp)
target_link_libraries(AedstFeatureNotificationsTests PRIVATE Aedst::Feature::Notifications GTest::gtest_main)
aedst_set_warnings(AedstFeatureNotificationsTests)

add_test(NAME AedstFeatureNotifications.UnitTests COMMAND AedstFeatureNotificationsTests)
set_tests_properties(AedstFeatureNotifications.UnitTests PROPERTIES LABELS "unit")
