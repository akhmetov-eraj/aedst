#pragma once

#include "aedst/navigation/route.hpp"
#include "aedst/plugin/feature_plugin.hpp"

#include <memory>
#include <string>
#include <string_view>

namespace aedst::feature::media {

inline constexpr std::string_view kMediaLibraryViewUrl = "qrc:/qt/qml/Aedst/Feature/Media/Views/MediaLibraryView.qml";
inline constexpr std::string_view kMediaViewerViewUrl = "qrc:/qt/qml/Aedst/Feature/Media/Views/MediaViewerView.qml";

[[nodiscard]] inline navigation::Route mediaLibraryRoute() {
    return navigation::Route{.name = std::string(navigation::routes::kMedia), .params = {}};
}

[[nodiscard]] inline navigation::Route mediaViewerRoute(std::string mediaId) {
    return navigation::Route{.name = std::string(navigation::routes::kMediaViewer),
                             .params = {{std::string(navigation::params::kMediaId), std::move(mediaId)}}};
}

/// This feature as a plugin (identity + screens); the composition root adds it to the PluginManager and
/// registers the routes of the active plugins in the NavGraph. See ADR-0033.
[[nodiscard]] std::unique_ptr<plugin::IFeaturePlugin> makeMediaPlugin();

} // namespace aedst::feature::media
