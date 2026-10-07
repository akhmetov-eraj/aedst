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
class AsyncTokenStore;
} // namespace aedst::persistence

namespace aedst::feature::auth {

/// Maps Session <-> persistence::StoredTokens, and doubles as the
/// network::TokenProvider AuthInterceptor reads — so the bearer token always
/// comes from the same place the session is persisted.
///
/// Threading: the database lives on its own thread (persistence::DatabaseWorker). This class never
/// touches it from the caller's thread: reads (load(), accessToken()) are served from an in-memory cache
/// and are therefore cheap enough for every outgoing request; preload(), save() and clear() are
/// asynchronous and complete on the dispatcher's thread (the GUI thread in the app).
///  - preload(): fills the cache from disk once at startup; until it completes, load() reports "no session".
///  - save(): the cache is updated only after the write succeeded, so a session that could not be stored is
///    never reported as signed in (the completion carries the storage error).
///  - clear(): the cache (and its tokens) is wiped at once, the on-disk wipe follows; the completion reports
///    the disk result. Sign-out therefore never leaves a usable token in memory while waiting for the disk.
///
/// The in-memory cache holds tokens in core::SecretString, which wipes on destruction/clear (logout). Copies
/// handed out through the public interface (Session, std::string) are owned by the caller and are outside this
/// class's control. Thread-safe: the cache is guarded by a mutex.
class LocalTokenStoreAdapter final : public network::TokenProvider,
                                     public std::enable_shared_from_this<LocalTokenStoreAdapter> {
public:
    explicit LocalTokenStoreAdapter(std::shared_ptr<persistence::AsyncTokenStore> store);

    /// Loads the stored session into the cache. Idempotent; a cache that already holds a session is kept.
    void preload(core::Completion<void> done);
    void save(const Session& session, core::Completion<void> done);
    void clear(core::Completion<void> done);

    /// The cached session; never reads the database.
    [[nodiscard]] core::Outcome<std::optional<Session>> load() const;

    [[nodiscard]] std::optional<std::string> accessToken() const override;

private:
    struct CachedSession {
        std::string userId;
        core::SecretString accessToken;
        core::SecretString refreshToken;
        std::chrono::system_clock::time_point accessTokenExpiresAt;
    };

    std::shared_ptr<persistence::AsyncTokenStore> store_;
    mutable std::mutex mutex_;
    std::optional<CachedSession> cached_;
};

} // namespace aedst::feature::auth
