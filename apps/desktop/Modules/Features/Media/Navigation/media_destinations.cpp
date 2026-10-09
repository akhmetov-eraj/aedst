#include "Navigation/media_destinations.hpp"

#include "aedst/navigation/route.hpp"

#include <memory>
#include <string>
#include <vector>

namespace aedst::feature::media {

std::unique_ptr<plugin::IFeaturePlugin> makeMediaPlugin() {
    return std::make_unique<plugin::StaticFeaturePlugin>(
        plugin::PluginDescriptor{.id = "media",
                                 .version = "1.0.0",
                                 .apiVersion = plugin::kPluginApiVersion,
                                 .requiredCapabilities = {},
                                 .featureFlag = {}},
        std::vector<plugin::RouteContribution>{
            {std::string(navigation::routes::kMedia), std::string(kMediaLibraryViewUrl), {"media"}},
            {std::string(navigation::routes::kMediaViewer), std::string(kMediaViewerViewUrl)},
        });
}

} // namespace aedst::feature::media
