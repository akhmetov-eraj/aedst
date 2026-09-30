#pragma once

#include "aedst/core/outcome.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace aedst::persistence {

class SqliteConnection;
class SecureStorage;

struct StoredTokens {
    std::string userId;
    std::string accessToken;
    std::string refreshToken;
    std::int64_t accessTokenExpiresAtEpochSeconds{0};

    friend bool operator==(const StoredTokens&, const StoredTokens&) = default;
};

/// Persists the single signed-in session. Token columns are SecureStorage
/// ciphertext; a SHA-256 fingerprint of the refresh token is stored alongside so
/// diagnostics can tell sessions apart without decrypting anything.
///
/// Session-agnostic on purpose: it lives in Core so the App composition root
/// (and features other than Auth, via an injected provider) can read "who is
/// signed in" without depending on Features/Auth.
///
/// Threading contract: the SQLite connection is FULLMUTEX and SecureStorage is
/// internally locked, so every method is safe to call from any thread. The app
/// calls them on the GUI thread on purpose: a save or load is two AES-GCM
/// operations plus one single-row statement on a local WAL database (well under a
/// millisecond; enforced by the PerformanceBudget test), so a worker thread would
/// add synchronisation for no user-visible gain. Do not add unbounded work here
/// without revisiting that.
class TokenStore final {
public:
    TokenStore(std::shared_ptr<SqliteConnection> connection, std::shared_ptr<const SecureStorage> secureStorage);

    /// Brings the database schema up to date with appMigrations() (all features'
    /// tables, not only auth_session). Fails if the database is newer than this
    /// build. Idempotent.
    [[nodiscard]] core::VoidOutcome migrate();
    [[nodiscard]] core::VoidOutcome save(const StoredTokens& tokens);
    [[nodiscard]] core::Outcome<std::optional<StoredTokens>> load() const;

    /// Logout wipe: deletes the session row (secure_delete zero-fills the freed
    /// page) and truncates the WAL so ciphertext does not linger on disk.
    [[nodiscard]] core::VoidOutcome clear();

private:
    std::shared_ptr<SqliteConnection> connection_;
    std::shared_ptr<const SecureStorage> secureStorage_;
};

} // namespace aedst::persistence
