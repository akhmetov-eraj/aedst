#pragma once

#include <cstdint>
#include <string>

namespace aedst::outbox {

enum class OutboxStatus {
    /// Waiting for delivery (possibly after retryable failures).
    Pending = 0,
    /// Parked: permanent failure, unknown type or attempts exhausted. Kept for diagnostics, never retried.
    Failed = 1,
};

/// One queued client -> server operation. `id` is also the Idempotency-Key of every delivery attempt, so a
/// retry after a lost response is applied once by the server.
///
/// NEVER put secrets, tokens or account data in `payload`: the database file is not encrypted (same rule as
/// Android's outbox, docs/content/decisions/0013-android-offline-and-client-security.md). What the user typed
/// into the operation itself (a task title) is the only user content that belongs there.
struct OutboxItem {
    std::string id;
    /// Routing key selecting the OutboxHandler that delivers `payload` (for example "task.create").
    std::string type;
    std::string payload;
    int attempts{0};
    OutboxStatus status{OutboxStatus::Pending};
    std::int64_t createdAtMs{0};
    /// Error KIND of the last failure (core::toString(ErrorCode) or "NoHandler"); never a message, which could
    /// carry user data.
    std::string lastError;

    friend bool operator==(const OutboxItem&, const OutboxItem&) = default;
};

} // namespace aedst::outbox
