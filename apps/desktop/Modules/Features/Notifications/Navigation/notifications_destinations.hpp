#pragma once

#include "aedst/navigation/route.hpp"
#include "aedst/plugin/feature_plugin.hpp"

#include <memory>
#include <string>
#include <string_view>

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

/// This feature as a plugin (identity + screens); the composition root adds it to the PluginManager and
/// registers the routes of the active plugins in the NavGraph. See ADR-0033.
[[nodiscard]] std::unique_ptr<plugin::IFeaturePlugin> makeNotificationsPlugin();

} // namespace aedst::feature::notifications
