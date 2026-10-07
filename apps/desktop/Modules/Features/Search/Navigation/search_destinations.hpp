#pragma once

#include "aedst/navigation/route.hpp"

#include <string>
#include <string_view>

namespace aedst::navigation {
class NavGraph;
} // namespace aedst::navigation

namespace aedst::feature::search {

/// qrc path from qt_add_qml_module(AedstFeatureSearchViews URI Aedst.Feature.Search.Views).
inline constexpr std::string_view kSearchViewUrl = "qrc:/qt/qml/Aedst/Feature/Search/Views/SearchView.qml";

[[nodiscard]] inline navigation::Route searchRoute() {
    return navigation::Route{.name = std::string(navigation::routes::kSearch), .params = {}};
}

/// Adds this feature's screens to the app-wide graph. Called only by App/.
void registerSearchDestinations(navigation::NavGraph& graph);

} // namespace aedst::feature::search
