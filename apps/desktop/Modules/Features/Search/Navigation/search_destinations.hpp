#pragma once

#include "aedst/navigation/route.hpp"
#include "aedst/plugin/feature_plugin.hpp"

#include <memory>
#include <string>
#include <string_view>

namespace aedst::feature::search {

/// qrc path from qt_add_qml_module(AedstFeatureSearchViews URI Aedst.Feature.Search.Views).
inline constexpr std::string_view kSearchViewUrl = "qrc:/qt/qml/Aedst/Feature/Search/Views/SearchView.qml";

[[nodiscard]] inline navigation::Route searchRoute() {
    return navigation::Route{.name = std::string(navigation::routes::kSearch), .params = {}};
}

/// This feature as a plugin (identity + screens); the composition root adds it to the PluginManager and
/// registers the routes of the active plugins in the NavGraph. See ADR-0033.
[[nodiscard]] std::unique_ptr<plugin::IFeaturePlugin> makeSearchPlugin();

} // namespace aedst::feature::search
