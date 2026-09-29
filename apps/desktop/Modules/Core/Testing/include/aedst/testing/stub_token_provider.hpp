#pragma once

#include "aedst/network/auth_interceptor.hpp"

#include <optional>
#include <string>
#include <utility>

namespace aedst::testing {

/// network::TokenProvider with a settable token, for exercising AuthInterceptor
/// and ApiClient without a TokenStore/SQLite behind them.
class StubTokenProvider final : public network::TokenProvider {
public:
    explicit StubTokenProvider(std::optional<std::string> token = std::nullopt) : token_(std::move(token)) {}

    [[nodiscard]] std::optional<std::string> accessToken() const override { return token_; }

    void setToken(std::optional<std::string> token) { token_ = std::move(token); }

private:
    std::optional<std::string> token_;
};

} // namespace aedst::testing
