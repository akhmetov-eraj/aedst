#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace aedst::navigation {

/// Well-known route names. A feature navigates to a name another feature
/// registers a destination for — never to that feature's types — the same way
/// a server module calls another module's RPC contract instead of importing its
/// handler package. New features add their route here, in Core.
namespace routes {

inline constexpr std::string_view kLogin = "auth/login";
inline constexpr std::string_view kTasks = "tasks/list";
inline constexpr std::string_view kTaskDetail = "tasks/detail";
inline constexpr std::string_view kSearch = "search/home";
inline constexpr std::string_view kNotifications = "notifications/inbox";
inline constexpr std::string_view kNotificationPreferences = "notifications/preferences";
inline constexpr std::string_view kMedia = "media/library";
inline constexpr std::string_view kMediaViewer = "media/viewer";
inline constexpr std::string_view kAdmin = "admin/home";

} // namespace routes

/// Well-known parameter keys shared across routes.
namespace params {

inline constexpr std::string_view kUserId = "userId";
inline constexpr std::string_view kTaskId = "taskId";
inline constexpr std::string_view kMediaId = "mediaId";

} // namespace params

struct Route {
    std::string name;
    /// Passed to the destination view as initial QML properties.
    std::map<std::string, std::string, std::less<>> params;

    friend bool operator==(const Route&, const Route&) = default;
};

/// Maps a server-provided in-app link (Notification.link, SearchHit.link,
/// BroadcastNotification.link: relative paths such as "/tasks/<id>") to a Route.
/// The contract says these are relative only; anything else (absolute URLs,
/// "//host", unknown prefixes, ids with path or query characters) yields nullopt
/// so a server string can never steer navigation outside the known graph.
/// Pure; defined in src/router.cpp.
[[nodiscard]] std::optional<Route> routeForLink(std::string_view link);

} // namespace aedst::navigation
