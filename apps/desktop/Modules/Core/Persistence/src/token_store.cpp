#include "aedst/persistence/token_store.hpp"

#include "aedst/persistence/secure_storage.hpp"
#include "aedst/persistence/sqlite_connection.hpp"

#include <cstdint>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace aedst::persistence {
namespace {

constexpr std::string_view kSchema = R"sql(
CREATE TABLE IF NOT EXISTS auth_session (
    id                  INTEGER PRIMARY KEY CHECK (id = 1),
    user_id             TEXT    NOT NULL,
    access_token        BLOB    NOT NULL,
    refresh_token       BLOB    NOT NULL,
    access_expires_at   INTEGER NOT NULL,
    refresh_fingerprint TEXT    NOT NULL
);
)sql";

std::span<const std::uint8_t> bytesOf(std::string_view text) {
    return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
}

std::string stringOf(const std::vector<std::uint8_t>& bytes) {
    return {bytes.begin(), bytes.end()};
}

} // namespace

TokenStore::TokenStore(std::shared_ptr<SqliteConnection> connection, std::shared_ptr<const SecureStorage> secureStorage)
    : connection_(std::move(connection)), secureStorage_(std::move(secureStorage)) {}

core::VoidOutcome TokenStore::migrate() {
    return connection_->execute(kSchema);
}

core::VoidOutcome TokenStore::save(const StoredTokens& tokens) {
    auto access = secureStorage_->protect(bytesOf(tokens.accessToken));
    if (!access) {
        return core::fail(access.error());
    }
    auto refresh = secureStorage_->protect(bytesOf(tokens.refreshToken));
    if (!refresh) {
        return core::fail(refresh.error());
    }
    auto fingerprint = SecureStorage::sha256Hex(tokens.refreshToken);
    if (!fingerprint) {
        return core::fail(fingerprint.error());
    }

    auto statement = connection_->prepare(
        "INSERT OR REPLACE INTO auth_session "
        "(id, user_id, access_token, refresh_token, access_expires_at, refresh_fingerprint) "
        "VALUES (1, ?1, ?2, ?3, ?4, ?5)");
    if (!statement) {
        return core::fail(statement.error());
    }
    for (auto bound : {statement->bind(1, tokens.userId), statement->bind(2, std::span<const std::uint8_t>(*access)),
                       statement->bind(3, std::span<const std::uint8_t>(*refresh)),
                       statement->bind(4, tokens.accessTokenExpiresAtEpochSeconds),
                       statement->bind(5, *fingerprint)}) {
        if (!bound) {
            return bound;
        }
    }
    if (auto stepped = statement->step(); !stepped) {
        return core::fail(stepped.error());
    }
    return core::ok();
}

core::Outcome<std::optional<StoredTokens>> TokenStore::load() const {
    auto statement = connection_->prepare(
        "SELECT user_id, access_token, refresh_token, access_expires_at FROM auth_session WHERE id = 1");
    if (!statement) {
        return core::fail(statement.error());
    }
    auto hasRow = statement->step();
    if (!hasRow) {
        return core::fail(hasRow.error());
    }
    if (!*hasRow) {
        return std::optional<StoredTokens>{};
    }

    auto access = secureStorage_->unprotect(statement->columnBlob(1));
    if (!access) {
        return core::fail(access.error());
    }
    auto refresh = secureStorage_->unprotect(statement->columnBlob(2));
    if (!refresh) {
        return core::fail(refresh.error());
    }

    return std::optional<StoredTokens>{StoredTokens{
        .userId = statement->columnText(0),
        .accessToken = stringOf(*access),
        .refreshToken = stringOf(*refresh),
        .accessTokenExpiresAtEpochSeconds = statement->columnInt64(3),
    }};
}

core::VoidOutcome TokenStore::clear() {
    return connection_->execute("DELETE FROM auth_session");
}

} // namespace aedst::persistence
