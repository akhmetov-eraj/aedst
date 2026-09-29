#pragma once

#include "Domain/session.hpp"
#include "aedst/core/outcome.hpp"
#include "aedst/network/auth_interceptor.hpp"

#include <memory>
#include <optional>
#include <string>

namespace aedst::persistence {
class TokenStore;
} // namespace aedst::persistence

namespace aedst::feature::auth {

/// Maps Session <-> persistence::StoredTokens, and doubles as the
/// network::TokenProvider AuthInterceptor reads — so the bearer token always
/// comes from the same place the session is persisted.
class LocalTokenStoreAdapter final : public network::TokenProvider {
public:
    explicit LocalTokenStoreAdapter(std::shared_ptr<persistence::TokenStore> store);

    [[nodiscard]] core::VoidOutcome save(const Session& session);
    [[nodiscard]] core::Outcome<std::optional<Session>> load() const;
    [[nodiscard]] core::VoidOutcome clear();

    [[nodiscard]] std::optional<std::string> accessToken() const override;

private:
    std::shared_ptr<persistence::TokenStore> store_;
    // Avoids a SQLite read + decrypt on every outgoing request.
    mutable std::optional<Session> cached_;
};

} // namespace aedst::feature::auth
