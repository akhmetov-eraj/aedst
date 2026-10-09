#include "Navigation/search_destinations.hpp"

#include "aedst/navigation/route.hpp"

#include <memory>
#include <string>
#include <vector>

namespace aedst::feature::search {

std::unique_ptr<plugin::IFeaturePlugin> makeSearchPlugin() {
    return std::make_unique<plugin::StaticFeaturePlugin>(
        plugin::PluginDescriptor{.id = "search",
                                 .version = "1.0.0",
                                 .apiVersion = plugin::kPluginApiVersion,
                                 .requiredCapabilities = {},
                                 .featureFlag = {}},
        std::vector<plugin::RouteContribution>{
            {std::string(navigation::routes::kSearch), std::string(kSearchViewUrl)},
        });
}

} // namespace aedst::feature::search
