#pragma once

#include "aedst/navigation/route.hpp"
#include "aedst/plugin/feature_plugin.hpp"

#include <memory>
#include <string>
#include <string_view>

namespace aedst::feature::admin {

inline constexpr std::string_view kAdminViewUrl = "qrc:/qt/qml/Aedst/Feature/Admin/Views/AdminView.qml";

/// The role every aedst.admin.v1 RPC requires (server: AEDST_ROLE_POLICY default).
inline constexpr std::string_view kAdminRole = "admin";

[[nodiscard]] inline navigation::Route adminRoute() {
    return navigation::Route{.name = std::string(navigation::routes::kAdmin), .params = {}};
}

/// This feature as a plugin (identity + screens); the composition root adds it to the PluginManager and
/// registers the routes of the active plugins in the NavGraph. See ADR-0033.
[[nodiscard]] std::unique_ptr<plugin::IFeaturePlugin> makeAdminPlugin();

} // namespace aedst::feature::admin
