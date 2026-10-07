#pragma once

#include "aedst/navigation/route.hpp"

#include <string>
#include <string_view>

namespace aedst::navigation {
class NavGraph;
} // namespace aedst::navigation

namespace aedst::feature::notifications {

inline constexpr std::string_view kNotificationsViewUrl =
    "qrc:/qt/qml/Aedst/Feature/Notifications/Views/NotificationsView.qml";
inline constexpr std::string_view kNotificationPreferencesViewUrl =
    "qrc:/qt/qml/Aedst/Feature/Notifications/Views/NotificationPreferencesView.qml";

[[nodiscard]] inline navigation::Route notificationsRoute() {
    return navigation::Route{.name = std::string(navigation::routes::kNotifications), .params = {}};
}

[[nodiscard]] inline navigation::Route notificationPreferencesRoute() {
    return navigation::Route{.name = std::string(navigation::routes::kNotificationPreferences), .params = {}};
}

/// Adds this feature's screens to the app-wide graph. Called only by App/.
void registerNotificationsDestinations(navigation::NavGraph& graph);

} // namespace aedst::feature::notifications
