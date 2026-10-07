#pragma once

#include "aedst/navigation/route.hpp"

#include <string>
#include <string_view>

namespace aedst::feature::auth {

/// qrc path of the AedstFeatureAuthViews QML module (URI Aedst.Feature.Auth.Views) in this feature's CMakeLists.txt.
inline constexpr std::string_view kLoginViewUrl = "qrc:/qt/qml/Aedst/Feature/Auth/Views/LoginView.qml";

[[nodiscard]] inline navigation::Route loginRoute() {
    return navigation::Route{.name = std::string(navigation::routes::kLogin), .params = {}};
}

} // namespace aedst::feature::auth
