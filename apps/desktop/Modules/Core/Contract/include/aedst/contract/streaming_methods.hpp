#pragma once

#include "aedst/contract/methods.hpp"

#include <array>
#include <string_view>

/// Server-streaming procedures (`rpc X(...) returns (stream Y)`), kept apart from the unary table in
/// methods.hpp because ConnectClient::serverStream and ConnectClient::unary are different entry points
/// and a descriptor test must assert `server_streaming()` for these, `!server_streaming()` for the rest.
///
/// An entry exists only once the server's proto declares the RPC: Contract's CMake sets
/// AEDST_CONTRACT_HAS_WATCH_NOTIFICATIONS when it finds `rpc WatchNotifications` in
/// notifications.proto. Until then the desktop polls (docs/proposals/notifications-watch-rpc.md).
namespace aedst::contract {

#if defined(AEDST_CONTRACT_HAS_WATCH_NOTIFICATIONS)
inline constexpr auto kNotificationsWatch =
    makeMethod<notificationsv1::WatchNotificationsRequest, notificationsv1::NotificationEvent>(
        "/aedst.notifications.v1.NotificationsService/WatchNotifications", AuthRequirement::Required);

/// Every server-streaming path above; compared with the generated descriptors by methods_test.cpp.
inline constexpr std::array kAllStreamingMethodPaths = std::to_array<std::string_view>({
    kNotificationsWatch.info.path,
});
#else
inline constexpr std::array<std::string_view, 0> kAllStreamingMethodPaths{};
#endif

} // namespace aedst::contract
