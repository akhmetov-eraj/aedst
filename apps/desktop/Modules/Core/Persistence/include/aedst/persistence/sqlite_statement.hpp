#pragma once

#include "aedst/core/outcome.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct sqlite3_stmt;

namespace aedst::persistence {

/// Owns one prepared statement. Bind indices are 1-based, column indices
/// 0-based, exactly as in the sqlite3 C API.
class SqliteStatement final {
public:
    explicit SqliteStatement(sqlite3_stmt* statement) noexcept;

    SqliteStatement(SqliteStatement&&) noexcept = default;
    SqliteStatement& operator=(SqliteStatement&&) noexcept = default;
    SqliteStatement(const SqliteStatement&) = delete;
    SqliteStatement& operator=(const SqliteStatement&) = delete;
    ~SqliteStatement() = default;

    [[nodiscard]] core::VoidOutcome bind(int index, std::string_view value);
    [[nodiscard]] core::VoidOutcome bind(int index, std::int64_t value);
    [[nodiscard]] core::VoidOutcome bind(int index, std::span<const std::uint8_t> blob);
    [[nodiscard]] core::VoidOutcome bindNull(int index);

    /// true when a row is available, false when the statement is done.
    [[nodiscard]] core::Outcome<bool> step();
    [[nodiscard]] core::VoidOutcome reset();

    [[nodiscard]] std::string columnText(int column) const;
    [[nodiscard]] std::int64_t columnInt64(int column) const;
    [[nodiscard]] std::vector<std::uint8_t> columnBlob(int column) const;
    [[nodiscard]] bool isNull(int column) const;

private:
    struct Finalizer {
        void operator()(sqlite3_stmt* statement) const noexcept;
    };

    [[nodiscard]] core::VoidOutcome check(int resultCode) const;

    std::unique_ptr<sqlite3_stmt, Finalizer> statement_;
};

} // namespace aedst::persistence
