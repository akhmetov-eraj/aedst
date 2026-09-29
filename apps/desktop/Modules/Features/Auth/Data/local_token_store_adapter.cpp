#include "Data/local_token_store_adapter.hpp"

#include "aedst/core/logger.hpp"
#include "aedst/persistence/token_store.hpp"

#include <chrono>
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

Session toSession(persistence::StoredTokens stored) {
    return {
        .userId = std::move(stored.userId),
        .accessToken = std::move(stored.accessToken),
        .refreshToken = std::move(stored.refreshToken),
        .accessTokenExpiresAt = std::chrono::system_clock::time_point{
            std::chrono::seconds{stored.accessTokenExpiresAtEpochSeconds}},
    };
}

} // namespace

LocalTokenStoreAdapter::LocalTokenStoreAdapter(std::shared_ptr<persistence::TokenStore> store)
    : store_(std::move(store)) {}

core::VoidOutcome LocalTokenStoreAdapter::save(const Session& session) {
    auto saved = store_->save(toStored(session));
    if (saved) {
        cached_ = session;
    }
    return saved;
}

core::Outcome<std::optional<Session>> LocalTokenStoreAdapter::load() const {
    if (cached_) {
        return cached_;
    }
    auto stored = store_->load();
    if (!stored) {
        return core::fail(stored.error());
    }
    if (!*stored) {
        return std::optional<Session>{};
    }
    cached_ = toSession(std::move(**stored));
    return cached_;
}

core::VoidOutcome LocalTokenStoreAdapter::clear() {
    cached_.reset();
    return store_->clear();
}

std::optional<std::string> LocalTokenStoreAdapter::accessToken() const {
    auto session = load();
    if (!session) {
        core::AedstLogger::error("Failed to read stored session: {}", session.error().message);
        return std::nullopt;
    }
    if (!*session) {
        return std::nullopt;
    }
    return (*session)->accessToken;
}

} // namespace aedst::feature::auth
