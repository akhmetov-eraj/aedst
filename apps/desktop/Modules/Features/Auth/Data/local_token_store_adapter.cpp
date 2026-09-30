#include "Data/local_token_store_adapter.hpp"

#include "aedst/core/logger.hpp"
#include "aedst/persistence/token_store.hpp"

#include <chrono>
#include <cstdint>
#include <utility>

namespace aedst::feature::auth {
namespace {

persistence::StoredTokens toStored(const Session& session) {
    return {
        .userId = session.userId,
        .accessToken = session.accessToken,
        .refreshToken = session.refreshToken,
        .accessTokenExpiresAtEpochSeconds =
            std::chrono::duration_cast<std::chrono::seconds>(session.accessTokenExpiresAt.time_since_epoch()).count(),
    };
}

std::chrono::system_clock::time_point toTimePoint(std::int64_t epochSeconds) {
    return std::chrono::system_clock::time_point{std::chrono::seconds{epochSeconds}};
}

} // namespace

LocalTokenStoreAdapter::LocalTokenStoreAdapter(std::shared_ptr<persistence::TokenStore> store)
    : store_(std::move(store)) {}

core::VoidOutcome LocalTokenStoreAdapter::save(const Session& session) {
    persistence::StoredTokens stored = toStored(session);
    auto saved = store_->save(stored);
    // Wipe the temporary plaintext copy made for persistence.
    core::secureWipe(stored.accessToken);
    core::secureWipe(stored.refreshToken);
    if (!saved) {
        return saved;
    }

    const std::scoped_lock lock(mutex_);
    cached_ = CachedSession{
        .userId = session.userId,
        .accessToken = core::SecretString{session.accessToken},
        .refreshToken = core::SecretString{session.refreshToken},
        .accessTokenExpiresAt = session.accessTokenExpiresAt,
    };
    return saved;
}

core::VoidOutcome LocalTokenStoreAdapter::ensureCachedLocked() const {
    if (cached_) {
        return core::ok();
    }
    auto stored = store_->load();
    if (!stored) {
        return core::fail(stored.error());
    }
    if (!*stored) {
        return core::ok();
    }
    // Moving the strings into SecretString transfers the buffers; nothing is left behind.
    cached_ = CachedSession{
        .userId = std::move((*stored)->userId),
        .accessToken = core::SecretString{std::move((*stored)->accessToken)},
        .refreshToken = core::SecretString{std::move((*stored)->refreshToken)},
        .accessTokenExpiresAt = toTimePoint((*stored)->accessTokenExpiresAtEpochSeconds),
    };
    return core::ok();
}

core::Outcome<std::optional<Session>> LocalTokenStoreAdapter::load() const {
    const std::scoped_lock lock(mutex_);
    if (auto ready = ensureCachedLocked(); !ready) {
        return core::fail(ready.error());
    }
    if (!cached_) {
        return std::optional<Session>{};
    }
    return std::optional<Session>{Session{
        .userId = cached_->userId,
        .accessToken = cached_->accessToken.reveal(),
        .refreshToken = cached_->refreshToken.reveal(),
        .accessTokenExpiresAt = cached_->accessTokenExpiresAt,
    }};
}

core::VoidOutcome LocalTokenStoreAdapter::clear() {
    {
        const std::scoped_lock lock(mutex_);
        cached_.reset(); // ~SecretString wipes both tokens
    }
    return store_->clear();
}

std::optional<std::string> LocalTokenStoreAdapter::accessToken() const {
    const std::scoped_lock lock(mutex_);
    if (auto ready = ensureCachedLocked(); !ready) {
        core::AedstLogger::error("Failed to read stored session: {}", ready.error().message);
        return std::nullopt;
    }
    if (!cached_) {
        return std::nullopt;
    }
    return cached_->accessToken.reveal();
}

} // namespace aedst::feature::auth
