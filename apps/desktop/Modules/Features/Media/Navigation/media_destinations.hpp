#pragma once

#include "aedst/navigation/route.hpp"

#include <string>
#include <string_view>

namespace aedst::navigation {
class NavGraph;
} // namespace aedst::navigation

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

/// Adds this feature's screens to the app-wide graph. Called only by App/.
/// The viewer route is also what Tasks pushes (by name) to open an attachment.
void registerMediaDestinations(navigation::NavGraph& graph);

} // namespace aedst::feature::media
