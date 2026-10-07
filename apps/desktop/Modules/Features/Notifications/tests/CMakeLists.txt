find_package(GTest REQUIRED)

# Pure Domain: which polled notifications become native desktop notifications.
add_executable(AedstFeatureNotificationsTests new_notification_detector_test.cpp)
target_link_libraries(AedstFeatureNotificationsTests PRIVATE Aedst::Feature::Notifications GTest::gtest_main)
aedst_set_warnings(AedstFeatureNotificationsTests)

add_test(NAME AedstFeatureNotifications.UnitTests COMMAND AedstFeatureNotificationsTests)
set_tests_properties(AedstFeatureNotifications.UnitTests PROPERTIES LABELS "unit")

# Realtime delivery: reconnect supervisor and the ViewModel's stream/polling switch, against scripted fakes
# (no server). One QCoreApplication for timers and signals; main() is in notification_realtime_test.cpp.
find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Test)
add_executable(AedstFeatureNotificationsRealtimeTests
    notification_realtime_test.cpp
    notifications_view_model_test.cpp
)
target_link_libraries(AedstFeatureNotificationsRealtimeTests PRIVATE
    Aedst::Feature::Notifications Aedst::Analytics Aedst::Platform Aedst::Testing Qt6::Test GTest::gtest)
aedst_set_warnings(AedstFeatureNotificationsRealtimeTests)

add_test(NAME AedstFeatureNotifications.RealtimeTests COMMAND AedstFeatureNotificationsRealtimeTests)
set_tests_properties(AedstFeatureNotifications.RealtimeTests PROPERTIES
    LABELS "unit"
    TIMEOUT 120
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
