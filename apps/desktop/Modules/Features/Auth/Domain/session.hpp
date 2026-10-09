#pragma once

#include "aedst/core/secure_memory.hpp"

#include <chrono>
#include <string>

namespace aedst::feature::auth {

/// A signed-in session as the rest of the app sees it. Mirrors the server's
/// auth api.TokenPair plus the user id Login returns. The tokens are SecretStrings: wiped when the session dies, never
/// printed by accident; `reveal()` at the single place that really needs the text.
struct Session {
    std::string userId;
    core::SecretString accessToken;
    core::SecretString refreshToken;
    std::chrono::system_clock::time_point accessTokenExpiresAt;

    [[nodiscard]] bool isAccessTokenExpired(std::chrono::system_clock::time_point now) const noexcept {
        return now >= accessTokenExpiresAt;
    }

    friend bool operator==(const Session&, const Session&) = default;
};

} // namespace aedst::feature::auth
