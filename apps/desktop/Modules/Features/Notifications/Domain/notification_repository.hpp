#pragma once

#include "Domain/notification.hpp"
#include "aedst/core/outcome.hpp"

#include <string>
#include <vector>

namespace aedst::feature::notifications {

/// Domain-owned port over aedst.notifications.v1.NotificationsService (the
/// caller's own inbox; user_id is never a request field).
class NotificationRepository {
public:
    NotificationRepository() = default;
    NotificationRepository(const NotificationRepository&) = delete;
    NotificationRepository& operator=(const NotificationRepository&) = delete;
    NotificationRepository(NotificationRepository&&) = delete;
    NotificationRepository& operator=(NotificationRepository&&) = delete;
    virtual ~NotificationRepository() = default;

    virtual void list(bool unreadOnly, const std::string& pageToken, int pageSize,
                      core::Completion<NotificationPage> done) = 0;
    virtual void unreadCount(core::Completion<int> done) = 0;
    virtual void markRead(const std::vector<std::string>& ids, core::Completion<void> done) = 0;
    virtual void markAllRead(core::Completion<int> done) = 0;
    virtual void preferences(core::Completion<NotificationPreferences> done) = 0;
    virtual void updatePreferences(const NotificationPreferences& preferences,
                                   core::Completion<NotificationPreferences> done) = 0;
};

} // namespace aedst::feature::notifications
