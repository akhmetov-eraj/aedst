#pragma once

#include "aedst/core/outcome.hpp"

#include <span>
#include <string_view>

namespace aedst::persistence {

class SqliteConnection;

/// One forward-only schema step. `sql` may hold several statements.
struct Migration {
    int version{0};
    std::string_view name;
    std::string_view sql;
};

/// Applies an ordered migration list to a database.
///
/// - The list must be numbered 1, 2, 3, ... with no gaps.
/// - The applied version lives in `PRAGMA user_version`, which SQLite rolls back
///   together with the transaction, so a migration and its version bump are one
///   atomic step. Each migration runs in its own BEGIN IMMEDIATE transaction.
/// - Re-running is a no-op. A database whose user_version is newer than the last
///   known migration (opened by a newer build) is refused, never downgraded.
///
/// The Migrator does not own the list: `migrations` must outlive it.
class Migrator final {
public:
    explicit Migrator(std::span<const Migration> migrations) noexcept : migrations_(migrations) {}

    /// Version this build expects (0 for an empty list).
    [[nodiscard]] int latestVersion() const noexcept;

    /// Brings the database to latestVersion(); returns the resulting version.
    [[nodiscard]] core::Outcome<int> migrate(SqliteConnection& connection) const;

private:
    std::span<const Migration> migrations_;
};

/// The application's full ordered migration list (one shared user_version per
/// database, so every feature's schema lives in this single list, owned by
/// Core/Persistence). Append new migrations; never edit or reorder shipped ones.
[[nodiscard]] std::span<const Migration> appMigrations() noexcept;

} // namespace aedst::persistence
