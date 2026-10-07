#include "Data/connect_notification_repository.hpp"

#include "aedst/contract/methods.hpp"
#include "aedst/contract/timestamps.hpp"
#include "aedst/network/connect_client.hpp"

#include <algorithm>
#include <utility>

namespace aedst::feature::notifications {
namespace {

[[nodiscard]] NotificationKind kindFromProto(notificationsv1::NotificationKind kind) {
    switch (kind) {
    case notificationsv1::NOTIFICATION_KIND_TASK_COMPLETED:
        return NotificationKind::TaskCompleted;
    case notificationsv1::NOTIFICATION_KIND_TASK_DUE_SOON:
        return NotificationKind::TaskDueSoon;
    case notificationsv1::NOTIFICATION_KIND_MEDIA_READY:
        return NotificationKind::MediaReady;
    case notificationsv1::NOTIFICATION_KIND_SYSTEM:
        return NotificationKind::System;
    default:
        break;
    }
    return NotificationKind::Unknown;
}

[[nodiscard]] NotificationPreferences preferencesFromProto(const notificationsv1::Preferences& proto) {
    return NotificationPreferences{.pushEnabled = proto.push_enabled(),
                                   .taskCompleted = proto.task_completed(),
                                   .taskDueSoon = proto.task_due_soon(),
                                   .mediaReady = proto.media_ready()};
}

} // namespace

ConnectNotificationRepository::ConnectNotificationRepository(std::shared_ptr<network::ConnectClient> client)
    : client_(std::move(client)) {}

Notification ConnectNotificationRepository::toDomain(const notificationsv1::Notification& notification) {
    return Notification{
        .id = notification.id(),
        .kind = kindFromProto(notification.kind()),
        .title = notification.title(),
        .body = notification.body(),
        .link = notification.link(),
        .subjectId = notification.subject_id(),
        .createdAt = contract::toTimePoint(notification.created_at()),
        .readAt = contract::toOptionalTimePoint(notification.has_read_at(), notification.read_at()),
    };
}

void ConnectNotificationRepository::list(bool unreadOnly, const std::string& pageToken, int pageSize,
                                         core::Completion<NotificationPage> done) {
    notificationsv1::ListNotificationsRequest request;
    request.set_unread_only(unreadOnly);
    request.set_page_token(pageToken);
    request.set_page_size(std::clamp(pageSize, 0, 100));
    client_->unary(contract::kNotificationsListNotifications, request,
                   [done = std::move(done)](network::ApiResult<notificationsv1::ListNotificationsResponse> result) {
                       if (!result) {
                           done(core::fail(result.error().toCoreError()));
                           return;
                       }
                       NotificationPage page;
                       for (const auto& notification : result->notifications()) {
                           page.notifications.push_back(toDomain(notification));
                       }
                       page.nextPageToken = result->next_page_token();
                       done(std::move(page));
                   });
}

void ConnectNotificationRepository::unreadCount(core::Completion<int> done) {
    client_->unary(contract::kNotificationsGetUnreadCount, notificationsv1::GetUnreadCountRequest{},
                   [done = std::move(done)](network::ApiResult<notificationsv1::GetUnreadCountResponse> result) {
                       if (!result) {
                           done(core::fail(result.error().toCoreError()));
                           return;
                       }
                       done(std::max(0, result->unread()));
                   });
}

void ConnectNotificationRepository::markRead(const std::vector<std::string>& ids, core::Completion<void> done) {
    notificationsv1::MarkReadRequest request;
    for (const auto& id : ids) {
        if (request.ids_size() == 100) { // max_items: 100
            break;
        }
        request.add_ids(id);
    }
    if (request.ids_size() == 0) {
        done(core::ok());
        return;
    }
    client_->unary(contract::kNotificationsMarkRead, request,
                   [done = std::move(done)](network::ApiResult<notificationsv1::MarkReadResponse> result) {
                       if (!result) {
                           done(core::fail(result.error().toCoreError()));
                           return;
                       }
                       done(core::ok());
                   });
}

void ConnectNotificationRepository::markAllRead(core::Completion<int> done) {
    client_->unary(contract::kNotificationsMarkAllRead, notificationsv1::MarkAllReadRequest{},
                   [done = std::move(done)](network::ApiResult<notificationsv1::MarkAllReadResponse> result) {
                       if (!result) {
                           done(core::fail(result.error().toCoreError()));
                           return;
                       }
                       done(result->marked());
                   });
}

void ConnectNotificationRepository::preferences(core::Completion<NotificationPreferences> done) {
    client_->unary(contract::kNotificationsGetPreferences, notificationsv1::GetPreferencesRequest{},
                   [done = std::move(done)](network::ApiResult<notificationsv1::GetPreferencesResponse> result) {
                       if (!result) {
                           done(core::fail(result.error().toCoreError()));
                           return;
                       }
                       done(preferencesFromProto(result->preferences()));
                   });
}

void ConnectNotificationRepository::updatePreferences(const NotificationPreferences& preferences,
                                                      core::Completion<NotificationPreferences> done) {
    notificationsv1::UpdatePreferencesRequest request;
    auto* proto = request.mutable_preferences();
    proto->set_push_enabled(preferences.pushEnabled);
    proto->set_task_completed(preferences.taskCompleted);
    proto->set_task_due_soon(preferences.taskDueSoon);
    proto->set_media_ready(preferences.mediaReady);
    client_->unary(contract::kNotificationsUpdatePreferences, request,
                   [done = std::move(done)](network::ApiResult<notificationsv1::UpdatePreferencesResponse> result) {
                       if (!result) {
                           done(core::fail(result.error().toCoreError()));
                           return;
                       }
                       done(preferencesFromProto(result->preferences()));
                   });
}

} // namespace aedst::feature::notifications
