#pragma once

#include "aedst/core/outcome.hpp"
#include "aedst/persistence/database_worker.hpp"
#include "aedst/persistence/master_key_provider.hpp"
#include "aedst/persistence/token_store.hpp"

#include <functional>
#include <memory>
#include <optional>

namespace aedst::persistence {

class SecureStorage;

/// TokenStore on the database thread. Every operation is queued on the DatabaseWorker, so the schema
/// migration, the keyring probe for the master key (Secret Service IPC on Linux), the AES-GCM work and
/// the SQLite statements all stay off the caller's thread; the completion arrives through the Dispatcher.
///
/// The master key provider is created lazily on the worker thread by `keyProvider` (never on the caller's
/// thread), and the SecureStorage built from it is only ever used there.
///
/// Used by the first-party login path (OIDC keeps its refresh token in the OS keychain instead).
class AsyncTokenStore final {
public:
    using KeyProviderFactory = std::function<std::unique_ptr<MasterKeyProvider>()>;

    AsyncTokenStore(std::shared_ptr<DatabaseWorker> worker, KeyProviderFactory keyProvider, Dispatcher dispatcher);
    ~AsyncTokenStore();

    AsyncTokenStore(const AsyncTokenStore&) = delete;
    AsyncTokenStore& operator=(const AsyncTokenStore&) = delete;
    AsyncTokenStore(AsyncTokenStore&&) = delete;
    AsyncTokenStore& operator=(AsyncTokenStore&&) = delete;

    /// nullopt = nothing stored.
    void load(core::Completion<std::optional<StoredTokens>> done);
    /// The caller's plaintext copy is not touched; the copy queued for the worker is wiped after use.
    void save(const StoredTokens& tokens, core::Completion<void> done);
    /// Deletes the session row and truncates the WAL (see TokenStore::clear).
    void clear(core::Completion<void> done);

    [[nodiscard]] const std::shared_ptr<DatabaseWorker>& worker() const noexcept { return worker_; }

private:
    struct WorkerState; // lives on the worker thread only

    std::shared_ptr<DatabaseWorker> worker_;
    Dispatcher dispatcher_;
    std::shared_ptr<WorkerState> state_;
};

} // namespace aedst::persistence
