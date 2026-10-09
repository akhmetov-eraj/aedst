#pragma once

#include "aedst/core/outcome.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace aedst::persistence {

class SqliteConnection;

/// Synchronous SQL for the `kv` table (migration 3): a tiny key/value cache for non-secret data that must survive a
/// restart, for example the last-known server capabilities. Plain functions over one connection; they run on the
/// database thread (DatabaseWorker::run) or, in tests, against an in-memory connection.
///
/// Not for secrets (tokens, keys): those go through SecureStorage / the OS keychain. Values are opaque blobs; the
/// writer owns their format and versioning.
namespace kv {

/// Inserts or replaces the value of `key`.
[[nodiscard]] core::VoidOutcome put(SqliteConnection& connection, std::string_view key,
                                    std::span<const std::uint8_t> value, std::int64_t nowMs);

/// nullopt = no such key.
[[nodiscard]] core::Outcome<std::optional<std::vector<std::uint8_t>>> get(SqliteConnection& connection,
                                                                          std::string_view key);

[[nodiscard]] core::VoidOutcome remove(SqliteConnection& connection, std::string_view key);

} // namespace kv
} // namespace aedst::persistence
