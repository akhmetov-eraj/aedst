#pragma once

#include "Domain/notification.hpp"

#include <chrono>
#include <cstddef>
#include <set>
#include <string>
#include <vector>

namespace aedst::feature::notifications {

/// Decides which unread notifications deserve a native desktop notification.
/// Desktop has no push channel, so the app polls; this keeps polling from
/// re-announcing anything:
///  - only notifications created after `since` (the session start) - the backlog
///    from while the app was closed is summarized by the badge, not replayed;
///  - each id at most once (remembered, bounded to kMaxRemembered);
///  - filtered by NotificationPreferences (pushEnabled + per-kind switches);
///  - at most kMaxIndividual returned per poll; `overflow` counts the rest so the
///    caller shows one "N new notifications" summary instead of a toast storm.
class NewNotificationDetector {
public:
    static constexpr std::size_t kMaxIndividual = 3;
    static constexpr std::size_t kMaxRemembered = 500;

    explicit NewNotificationDetector(std::chrono::system_clock::time_point since) : since_(since) {}

    struct Result {
        std::vector<Notification> announce;
        std::size_t overflow{0};
    };

    [[nodiscard]] Result detect(const std::vector<Notification>& unread, const NotificationPreferences& preferences);

    /// New session (sign-in): forget everything and start counting from `since`.
    void reset(std::chrono::system_clock::time_point since);

private:
    std::chrono::system_clock::time_point since_;
    std::set<std::string> announced_;
};

} // namespace aedst::feature::notifications
