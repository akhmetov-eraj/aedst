#include "Domain/new_notification_detector.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <vector>

namespace aedst::feature::notifications {
namespace {

using namespace std::chrono_literals;

const auto kSessionStart = std::chrono::system_clock::time_point{std::chrono::seconds{1'000'000}};

Notification make(std::string id, std::chrono::seconds afterStart,
                  NotificationKind kind = NotificationKind::TaskCompleted) {
    return Notification{.id = std::move(id), .kind = kind, .title = "t", .createdAt = kSessionStart + afterStart};
}

TEST(NewNotificationDetectorTest, AnnouncesOnlyNewOnes) {
    NewNotificationDetector detector(kSessionStart);
    const NotificationPreferences preferences;

    const auto first = detector.detect({make("backlog", -60s), make("a", 5s)}, preferences);
    ASSERT_EQ(first.announce.size(), 1U);
    EXPECT_EQ(first.announce.front().id, "a");

    // Polling again with the same unread set announces nothing twice.
    EXPECT_TRUE(detector.detect({make("backlog", -60s), make("a", 5s)}, preferences).announce.empty());
}

TEST(NewNotificationDetectorTest, HonoursPreferences) {
    NewNotificationDetector detector(kSessionStart);
    NotificationPreferences preferences;
    preferences.taskCompleted = false;
    const auto result = detector.detect(
        {make("done", 1s, NotificationKind::TaskCompleted), make("sys", 2s, NotificationKind::System)}, preferences);
    ASSERT_EQ(result.announce.size(), 1U);
    EXPECT_EQ(result.announce.front().id, "sys");

    preferences.pushEnabled = false;
    EXPECT_TRUE(detector.detect({make("sys2", 3s, NotificationKind::System)}, preferences).announce.empty());
}

TEST(NewNotificationDetectorTest, CapsIndividualToastsAndCountsTheRest) {
    NewNotificationDetector detector(kSessionStart);
    std::vector<Notification> burst;
    for (int i = 0; i < 7; ++i) {
        burst.push_back(make("n" + std::to_string(i), std::chrono::seconds{i + 1}));
    }
    const auto result = detector.detect(burst, NotificationPreferences{});
    EXPECT_EQ(result.announce.size(), NewNotificationDetector::kMaxIndividual);
    EXPECT_EQ(result.overflow, 4U);
    EXPECT_EQ(result.announce.back().id, "n6"); // newest last
}

TEST(NewNotificationDetectorTest, ResetStartsANewSession) {
    NewNotificationDetector detector(kSessionStart);
    (void)detector.detect({make("a", 1s)}, NotificationPreferences{});
    detector.reset(kSessionStart + 10s);
    EXPECT_TRUE(detector.detect({make("a", 1s)}, NotificationPreferences{}).announce.empty()); // older than session
    EXPECT_EQ(detector.detect({make("b", 11s)}, NotificationPreferences{}).announce.size(), 1U);
}

} // namespace
} // namespace aedst::feature::notifications
