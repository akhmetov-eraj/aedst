#include "Navigation/admin_destinations.hpp"

#include "aedst/navigation/route.hpp"

#include <memory>
#include <string>
#include <vector>

namespace aedst::feature::admin {

std::unique_ptr<plugin::IFeaturePlugin> makeAdminPlugin() {
    return std::make_unique<plugin::StaticFeaturePlugin>(
        plugin::PluginDescriptor{.id = "admin",
                                 .version = "1.0.0",
                                 .apiVersion = plugin::kPluginApiVersion,
                                 .requiredCapabilities = {},
                                 .featureFlag = {}},
        std::vector<plugin::RouteContribution>{
            {std::string(navigation::routes::kAdmin), std::string(kAdminViewUrl)},
        });
}

} // namespace aedst::feature::admin
