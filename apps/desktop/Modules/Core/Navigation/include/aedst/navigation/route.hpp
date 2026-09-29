#pragma once

#include <functional>
#include <map>
#include <string>
#include <string_view>

namespace aedst::navigation {

/// Well-known route names. A feature navigates to a name another feature
/// registers a destination for — never to that feature's types — the same way
/// a server module calls another module's RPC contract instead of importing its
/// handler package. New features add their route here, in Core.
namespace routes {

inline constexpr std::string_view kLogin = "auth/login";

} // namespace routes

/// Well-known parameter keys shared across routes.
namespace params {

inline constexpr std::string_view kUserId = "userId";

} // namespace params

struct Route {
    std::string name;
    /// Passed to the destination view as initial QML properties.
    std::map<std::string, std::string, std::less<>> params;

    friend bool operator==(const Route&, const Route&) = default;
};

} // namespace aedst::navigation
