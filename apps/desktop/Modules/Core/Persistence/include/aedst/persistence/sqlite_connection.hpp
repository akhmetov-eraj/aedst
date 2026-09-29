#pragma once

#include "aedst/core/outcome.hpp"
#include "aedst/persistence/sqlite_statement.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

struct sqlite3;

namespace aedst::persistence {

/// Owns one sqlite3 connection (opened FULLMUTEX, WAL, foreign keys on).
/// Shared between TokenStore and feature caches via std::shared_ptr; all
/// access happens on the GUI thread today.
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

    [[nodiscard]] sqlite3* handle() const noexcept;
    [[nodiscard]] std::string lastErrorMessage() const;

private:
    struct Closer {
        void operator()(sqlite3* database) const noexcept;
    };

    explicit SqliteConnection(sqlite3* database) noexcept;

    std::unique_ptr<sqlite3, Closer> database_;
};

} // namespace aedst::persistence
