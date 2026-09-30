#pragma once

#include "aedst/core/outcome.hpp"
#include "aedst/persistence/sqlite_statement.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

struct sqlite3;

namespace aedst::persistence {

class Transaction;

/// Owns one sqlite3 connection (opened FULLMUTEX, WAL, foreign keys on,
/// secure_delete on, synchronous=NORMAL, 5 s busy timeout).
///
/// Threading: FULLMUTEX serialises every call into the connection, so it is
/// safe from any thread. It is used from the GUI thread today. A Transaction
/// spans several calls and is NOT isolated from other threads sharing the same
/// connection, so keep a Transaction's lifetime on a single thread.
class SqliteConnection final {
public:
    [[nodiscard]] static core::Outcome<SqliteConnection> open(const std::filesystem::path& file);
    [[nodiscard]] static core::Outcome<SqliteConnection> openInMemory();

    SqliteConnection(SqliteConnection&&) noexcept = default;
    SqliteConnection& operator=(SqliteConnection&&) noexcept = default;
    SqliteConnection(const SqliteConnection&) = delete;
    SqliteConnection& operator=(const SqliteConnection&) = delete;
    ~SqliteConnection() = default;

    /// Runs one or more statements with no result rows (DDL, PRAGMA, ...).
    [[nodiscard]] core::VoidOutcome execute(std::string_view sql);
    [[nodiscard]] core::Outcome<SqliteStatement> prepare(std::string_view sql);

    /// Starts a write transaction (BEGIN IMMEDIATE). Rolls back on destruction
    /// unless Transaction::commit() succeeded.
    [[nodiscard]] core::Outcome<Transaction> beginTransaction();

    /// Checkpoints the WAL and truncates it to zero bytes so pages deleted under
    /// secure_delete do not linger in the -wal file. No-op outside WAL mode.
    [[nodiscard]] core::VoidOutcome truncateWal();

    [[nodiscard]] sqlite3* handle() const noexcept;
    [[nodiscard]] std::string lastErrorMessage() const;

private:
    struct Closer {
        void operator()(sqlite3* database) const noexcept;
    };

    explicit SqliteConnection(sqlite3* database) noexcept;

    std::unique_ptr<sqlite3, Closer> database_;
};

/// RAII transaction. Move-only; a moved-from or finished transaction is inert.
class Transaction final {
public:
    Transaction(Transaction&& other) noexcept;
    Transaction& operator=(Transaction&& other) noexcept;
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    ~Transaction();

    /// On failure (e.g. SQLITE_BUSY) the transaction stays active and is rolled
    /// back on destruction.
    [[nodiscard]] core::VoidOutcome commit();
    void rollback() noexcept;

private:
    friend class SqliteConnection;
    explicit Transaction(SqliteConnection* connection) noexcept;

    SqliteConnection* connection_{nullptr};
};

} // namespace aedst::persistence
