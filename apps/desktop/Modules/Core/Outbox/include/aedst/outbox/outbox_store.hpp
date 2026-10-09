#pragma once

#include "aedst/core/outcome.hpp"
#include "aedst/outbox/outbox_item.hpp"
#include "aedst/persistence/sqlite_connection.hpp"

#include <string_view>
#include <vector>

/// Synchronous SQL for the `outbox` table (migration 2 in Core/Persistence). Plain functions over one connection:
/// they run on the database thread (SqliteOutboxQueue) or, in tests, against an in-memory connection.
namespace aedst::outbox::store {

/// INSERT OR IGNORE: an id that already exists leaves the stored row untouched (a double tap).
[[nodiscard]] core::VoidOutcome enqueue(persistence::SqliteConnection& connection, const OutboxItem& item);

/// Pending items in creation order, at most `limit`.
[[nodiscard]] core::Outcome<std::vector<OutboxItem>> pending(persistence::SqliteConnection& connection, int limit);

/// Every row (pending and parked) in creation order. Diagnostics and tests.
[[nodiscard]] core::Outcome<std::vector<OutboxItem>> all(persistence::SqliteConnection& connection);

[[nodiscard]] core::Outcome<int> pendingCount(persistence::SqliteConnection& connection);

/// Delivered: the row is deleted.
[[nodiscard]] core::VoidOutcome remove(persistence::SqliteConnection& connection, std::string_view id);

/// Retryable failure: attempts + 1, remember the error kind; the item stays pending.
[[nodiscard]] core::VoidOutcome recordAttempt(persistence::SqliteConnection& connection, std::string_view id,
                                              std::string_view errorKind);

/// Give up on the item: status = Failed (attempts is left as it is), remember the error kind.
[[nodiscard]] core::VoidOutcome markFailed(persistence::SqliteConnection& connection, std::string_view id,
                                           std::string_view errorKind);

/// Deletes every row and truncates the WAL so deleted payloads do not linger in the -wal file (sign-out).
[[nodiscard]] core::VoidOutcome clear(persistence::SqliteConnection& connection);

} // namespace aedst::outbox::store
