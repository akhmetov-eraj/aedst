#pragma once

#include "aedst/navigation/route.hpp"

#include <string>
#include <string_view>

namespace aedst::feature::auth {

/// qrc path set by qt_add_resources(... PREFIX "/aedst/feature/auth") in this
/// feature's CMakeLists.txt.
inline constexpr std::string_view kLoginViewUrl = "qrc:/aedst/feature/auth/LoginView.qml";

[[nodiscard]] inline navigation::Route loginRoute() {
    return navigation::Route{.name = std::string(navigation::routes::kLogin), .params = {}};
}

} // namespace aedst::feature::auth
