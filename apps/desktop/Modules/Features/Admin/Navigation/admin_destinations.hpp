#pragma once

#include "aedst/navigation/route.hpp"

#include <string>
#include <string_view>

namespace aedst::navigation {
class NavGraph;
} // namespace aedst::navigation

namespace aedst::feature::admin {

inline constexpr std::string_view kAdminViewUrl = "qrc:/qt/qml/Aedst/Feature/Admin/Views/AdminView.qml";

/// The role every aedst.admin.v1 RPC requires (server: AEDST_ROLE_POLICY default).
inline constexpr std::string_view kAdminRole = "admin";

[[nodiscard]] inline navigation::Route adminRoute() {
    return navigation::Route{.name = std::string(navigation::routes::kAdmin), .params = {}};
}

/// Adds this feature's screens to the app-wide graph. Called only by App/.
void registerAdminDestinations(navigation::NavGraph& graph);

} // namespace aedst::feature::admin
