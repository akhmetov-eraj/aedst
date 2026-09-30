#pragma once

#include "Domain/session.hpp"
#include "aedst/core/outcome.hpp"
#include "aedst/core/secure_memory.hpp"
#include "aedst/network/auth_interceptor.hpp"

#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace aedst::persistence {
class TokenStore;
} // namespace aedst::persistence

namespace aedst::feature::auth {

/// Maps Session <-> persistence::StoredTokens, and doubles as the
/// network::TokenProvider AuthInterceptor reads — so the bearer token always
/// comes from the same place the session is persisted.
///
/// The in-memory cache holds tokens in core::SecretString, which wipes on
/// destruction/clear (logout). Copies handed out through the public interface
/// (Session, std::string) are owned by the caller and are outside this class's
/// control. Thread-safe: the cache is guarded by a mutex.
class LocalTokenStoreAdapter final : public network::TokenProvider {
public:
    explicit LocalTokenStoreAdapter(std::shared_ptr<persistence::TokenStore> store);

    [[nodiscard]] core::VoidOutcome save(const Session& session);
    [[nodiscard]] core::Outcome<std::optional<Session>> load() const;
    [[nodiscard]] core::VoidOutcome clear();

    [[nodiscard]] std::optional<std::string> accessToken() const override;

private:
    struct CachedSession {
        std::string userId;
        core::SecretString accessToken;
        core::SecretString refreshToken;
        std::chrono::system_clock::time_point accessTokenExpiresAt;
    };

    /// Fills cached_ from the store when empty. Caller holds mutex_.
    [[nodiscard]] core::VoidOutcome ensureCachedLocked() const;

    std::shared_ptr<persistence::TokenStore> store_;
    mutable std::mutex mutex_;
    // Avoids a SQLite read + decrypt on every outgoing request.
    mutable std::optional<CachedSession> cached_;
};

} // namespace aedst::feature::auth
