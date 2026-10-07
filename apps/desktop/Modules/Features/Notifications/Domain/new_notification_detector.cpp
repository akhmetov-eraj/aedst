#include "Domain/new_notification_detector.hpp"

#include <algorithm>

namespace aedst::feature::notifications {

NewNotificationDetector::Result NewNotificationDetector::detect(const std::vector<Notification>& unread,
                                                                const NotificationPreferences& preferences) {
    std::vector<const Notification*> fresh;
    for (const Notification& notification : unread) {
        if (notification.isRead() || notification.createdAt < since_ || announced_.contains(notification.id)) {
            continue;
        }
        announced_.insert(notification.id); // remembered even if filtered: never announce it later
        if (preferences.allows(notification.kind)) {
            fresh.push_back(&notification);
        }
    }
    if (announced_.size() > kMaxRemembered) {
        announced_.clear(); // ids are only compared against newer-than-`since_` rows; a reset is harmless
        for (const Notification& notification : unread) {
            announced_.insert(notification.id);
        }
    }
    // Oldest first, so the newest notification is the last (topmost) one shown.
    std::ranges::sort(fresh, [](const Notification* a, const Notification* b) { return a->createdAt < b->createdAt; });

    Result result;
    const std::size_t individual = std::min(fresh.size(), kMaxIndividual);
    result.overflow = fresh.size() - individual;
    for (std::size_t i = fresh.size() - individual; i < fresh.size(); ++i) {
        result.announce.push_back(*fresh[i]);
    }
    return result;
}

void NewNotificationDetector::reset(std::chrono::system_clock::time_point since) {
    since_ = since;
    announced_.clear();
}

} // namespace aedst::feature::notifications
