#include "Data/connect_notification_stream.hpp"

#if defined(AEDST_CONTRACT_HAS_WATCH_NOTIFICATIONS)

#include "Data/connect_notification_repository.hpp"
#include "aedst/contract/streaming_methods.hpp"
#include "aedst/network/connect_client.hpp"

#include <optional>
#include <utility>

namespace aedst::feature::notifications {
namespace {

/// nullopt = an event kind this client does not know: skipped, never an error.
[[nodiscard]] std::optional<NotificationEvent> toDomain(const notificationsv1::NotificationEvent& message) {
    NotificationEvent event;
    event.id = message.event_id();
    switch (message.event_case()) {
    case notificationsv1::NotificationEvent::kCreated:
        event.payload = NotificationCreatedEvent{ConnectNotificationRepository::toDomain(message.created())};
        return event;
    case notificationsv1::NotificationEvent::kUnreadCount:
        event.payload = UnreadCountEvent{message.unread_count().unread()};
        return event;
    case notificationsv1::NotificationEvent::kHeartbeat:
        event.id.clear(); // heartbeats carry no resume point
        event.payload = HeartbeatEvent{};
        return event;
    case notificationsv1::NotificationEvent::kResyncRequired:
        event.payload = ResyncRequiredEvent{};
        return event;
    case notificationsv1::NotificationEvent::EVENT_NOT_SET:
        break;
    }
    return std::nullopt;
}

[[nodiscard]] StreamEnd toEnd(const network::ApiResult<void>& result) {
    if (result) {
        return StreamEnd{.kind = StreamEndKind::Ended, .message = "closed by the server"};
    }
    const network::ApiError& error = result.error();
    if (error.connectCode == "unimplemented" || error.httpStatus == 404) {
        return StreamEnd{.kind = StreamEndKind::Unsupported, .message = error.message};
    }
    switch (error.kind) {
    case network::ApiErrorKind::Unauthenticated:
        return StreamEnd{.kind = StreamEndKind::Unauthenticated, .message = error.message};
    case network::ApiErrorKind::InvalidArgument:
    case network::ApiErrorKind::PermissionDenied:
    case network::ApiErrorKind::NotFound:
        return StreamEnd{.kind = StreamEndKind::Rejected, .message = error.message};
    case network::ApiErrorKind::Cancelled:
        return StreamEnd{.kind = StreamEndKind::Transient, .message = "cancelled"};
    case network::ApiErrorKind::Transport:
    case network::ApiErrorKind::Timeout:
    case network::ApiErrorKind::AlreadyExists:
    case network::ApiErrorKind::Unavailable:
    case network::ApiErrorKind::Internal:
    case network::ApiErrorKind::Decoding:
    case network::ApiErrorKind::Unknown:
        break;
    }
    return StreamEnd{.kind = StreamEndKind::Transient, .message = error.message};
}

} // namespace

ConnectNotificationStream::ConnectNotificationStream(std::shared_ptr<network::ConnectClient> client)
    : client_(std::move(client)) {}

NotificationStream::Cancel ConnectNotificationStream::open(const std::string& lastEventId, Callbacks callbacks) {
    notificationsv1::WatchNotificationsRequest request;
    request.set_last_event_id(lastEventId);
    // No per-call timeout: a stream is meant to live; the supervisor's idle watchdog detects dead connections.
    network::CallHandle handle = client_->serverStream(
        contract::kNotificationsWatch, request,
        [onEvent = std::move(callbacks.onEvent)](notificationsv1::NotificationEvent message) {
            if (auto event = toDomain(message)) {
                onEvent(std::move(*event));
            }
        },
        [onEnd = std::move(callbacks.onEnd)](network::ApiResult<void> result) { onEnd(toEnd(result)); });
    return [handle]() { handle.cancelSilently(); };
}

} // namespace aedst::feature::notifications

#endif
