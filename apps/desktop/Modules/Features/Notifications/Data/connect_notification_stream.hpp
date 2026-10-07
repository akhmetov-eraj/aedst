#pragma once

#include "Domain/notification_stream.hpp"

#include <memory>

namespace aedst::network {
class ConnectClient;
} // namespace aedst::network

namespace aedst::feature::notifications {

#if defined(AEDST_CONTRACT_HAS_WATCH_NOTIFICATIONS)

/// NotificationStream over the server-streaming aedst.notifications.v1.NotificationsService/WatchNotifications.
///
/// Exists only when the server's proto declares that RPC (docs/proposals/notifications-watch-rpc.md);
/// Contract's CMake defines AEDST_CONTRACT_HAS_WATCH_NOTIFICATIONS then. Maps messages to domain events
/// (events with an unknown payload are ignored: forward compatibility), and the end of the stream to a
/// StreamEnd the supervisor can act on:
///   clean end-of-stream            -> Ended
///   unauthenticated (after the interceptor's one refresh) -> Unauthenticated
///   unimplemented / HTTP 404       -> Unsupported
///   invalid_argument, permission_denied, failed_precondition -> Rejected
///   everything else (transport, timeout, unavailable, internal) -> Transient
class ConnectNotificationStream final : public NotificationStream {
public:
    explicit ConnectNotificationStream(std::shared_ptr<network::ConnectClient> client);

    [[nodiscard]] Cancel open(const std::string& lastEventId, Callbacks callbacks) override;

private:
    std::shared_ptr<network::ConnectClient> client_;
};

#endif

} // namespace aedst::feature::notifications
