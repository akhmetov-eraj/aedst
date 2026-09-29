#pragma once

#include <chrono>
#include <string>

namespace aedst::feature::auth {

/// A signed-in session as the rest of the app sees it. Mirrors the server's
/// auth api.TokenPair plus the user id Login returns.
struct Session {
    std::string userId;
    std::string accessToken;
    std::string refreshToken;
    std::chrono::system_clock::time_point accessTokenExpiresAt;

    [[nodiscard]] bool isAccessTokenExpired(std::chrono::system_clock::time_point now) const noexcept {
        return now >= accessTokenExpiresAt;
    }

    friend bool operator==(const Session&, const Session&) = default;
};

} // namespace aedst::feature::auth
