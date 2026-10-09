#include "Navigation/notifications_destinations.hpp"

#include "aedst/navigation/route.hpp"

#include <memory>
#include <string>
#include <vector>

namespace aedst::feature::notifications {

std::unique_ptr<plugin::IFeaturePlugin> makeNotificationsPlugin() {
    return std::make_unique<plugin::StaticFeaturePlugin>(
        plugin::PluginDescriptor{.id = "notifications",
                                 .version = "1.0.0",
                                 .apiVersion = plugin::kPluginApiVersion,
                                 .requiredCapabilities = {},
                                 .featureFlag = {}},
        std::vector<plugin::RouteContribution>{
            {std::string(navigation::routes::kNotifications), std::string(kNotificationsViewUrl), {"notifications"}},
            {std::string(navigation::routes::kNotificationPreferences), std::string(kNotificationPreferencesViewUrl)},
        });
}

} // namespace aedst::feature::notifications
