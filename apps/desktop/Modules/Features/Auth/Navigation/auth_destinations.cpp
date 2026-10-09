#include "Navigation/auth_destinations.hpp"

#include "Navigation/auth_route.hpp"
#include "aedst/navigation/route.hpp"

#include <memory>
#include <string>
#include <vector>

namespace aedst::feature::auth {

std::unique_ptr<plugin::IFeaturePlugin> makeAuthPlugin() {
    return std::make_unique<plugin::StaticFeaturePlugin>(
        plugin::PluginDescriptor{.id = "auth",
                                 .version = "1.0.0",
                                 .apiVersion = plugin::kPluginApiVersion,
                                 .requiredCapabilities = {},
                                 .featureFlag = {}},
        std::vector<plugin::RouteContribution>{
            {std::string(navigation::routes::kLogin), std::string(kLoginViewUrl)},
        });
}

} // namespace aedst::feature::auth
