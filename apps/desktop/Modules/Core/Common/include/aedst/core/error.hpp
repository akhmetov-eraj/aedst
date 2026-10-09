#pragma once

#include <string>
#include <string_view>
#include <utility>

namespace aedst::core {

/// Transport-agnostic error categories. Network/persistence errors are mapped
/// into these at the Data layer so Domain and Presentation never see
/// QNetworkReply or sqlite3 codes.
///
/// The cases are the shared error kinds of docs/contract/behavior/error-kinds.json (replayed by
/// Modules/Core/Network/tests/error_kinds_replay_test.cpp); toString() returns the canonical lower-camel name.
enum class ErrorCode {
    Unknown,
    InvalidInput,
    Unauthenticated,
    /// Signed in, but the server refused the call for this identity (missing role).
    /// Distinct from Unauthenticated so a forbidden admin call never signs the user out.
    Forbidden,
    NotFound,
    /// The resource changed or already exists (Connect aborted / already_exists): re-read and retry.
    Conflict,
    /// Throttled by the server (Connect resource_exhausted); retry later.
    RateLimited,
    /// The server failed or could not be reached while the device is online (5xx, connection reset).
    Unavailable,
    /// The device has no usable connection.
    Offline,
    /// The call ran out of time (Connect deadline_exceeded, client timeout).
    Timeout,
    Storage,
    Cancelled,
    /// The server refuses this app version (failed_precondition / CLIENT_UPGRADE_REQUIRED). Not a failure of
    /// the request itself: the app shows a blocking upgrade screen, and retrying is pointless.
    UpgradeRequired,
};

[[nodiscard]] constexpr std::string_view toString(ErrorCode code) noexcept {
    switch (code) {
    case ErrorCode::InvalidInput:
        return "invalidInput";
    case ErrorCode::Unauthenticated:
        return "unauthenticated";
    case ErrorCode::Forbidden:
        return "forbidden";
    case ErrorCode::NotFound:
        return "notFound";
    case ErrorCode::Conflict:
        return "conflict";
    case ErrorCode::RateLimited:
        return "rateLimited";
    case ErrorCode::Unavailable:
        return "unavailable";
    case ErrorCode::Offline:
        return "offline";
    case ErrorCode::Timeout:
        return "timeout";
    case ErrorCode::Storage:
        return "storage";
    case ErrorCode::Cancelled:
        return "cancelled";
    case ErrorCode::UpgradeRequired:
        return "upgradeRequired";
    case ErrorCode::Unknown:
        break;
    }
    return "unknown";
}

/// Transient failures: the same request may succeed later (backoff, outbox retry). Same flags as error-kinds.json.
[[nodiscard]] constexpr bool isRetryable(ErrorCode code) noexcept {
    return code == ErrorCode::RateLimited || code == ErrorCode::Unavailable || code == ErrorCode::Offline ||
           code == ErrorCode::Timeout;
}

/// Worth a crash-reporter event: a bug or a storage fault, never an expected state. Same flags as error-kinds.json.
[[nodiscard]] constexpr bool isReportable(ErrorCode code) noexcept {
    return code == ErrorCode::Unknown || code == ErrorCode::Storage;
}

struct Error {
    ErrorCode code{ErrorCode::Unknown};
    std::string message;

    [[nodiscard]] static Error invalidInput(std::string text) { return {ErrorCode::InvalidInput, std::move(text)}; }

    [[nodiscard]] static Error unauthenticated(std::string text) {
        return {ErrorCode::Unauthenticated, std::move(text)};
    }

    [[nodiscard]] static Error forbidden(std::string text) { return {ErrorCode::Forbidden, std::move(text)}; }

    [[nodiscard]] static Error notFound(std::string text) { return {ErrorCode::NotFound, std::move(text)}; }

    [[nodiscard]] static Error conflict(std::string text) { return {ErrorCode::Conflict, std::move(text)}; }

    [[nodiscard]] static Error rateLimited(std::string text) { return {ErrorCode::RateLimited, std::move(text)}; }

    [[nodiscard]] static Error unavailable(std::string text) { return {ErrorCode::Unavailable, std::move(text)}; }

    [[nodiscard]] static Error offline(std::string text) { return {ErrorCode::Offline, std::move(text)}; }

    [[nodiscard]] static Error timeout(std::string text) { return {ErrorCode::Timeout, std::move(text)}; }

    [[nodiscard]] static Error storage(std::string text) { return {ErrorCode::Storage, std::move(text)}; }

    [[nodiscard]] static Error cancelled(std::string text) { return {ErrorCode::Cancelled, std::move(text)}; }

    [[nodiscard]] static Error upgradeRequired(std::string text) {
        return {ErrorCode::UpgradeRequired, std::move(text)};
    }

    [[nodiscard]] static Error unknown(std::string text) { return {ErrorCode::Unknown, std::move(text)}; }

    friend bool operator==(const Error&, const Error&) = default;
};

} // namespace aedst::core
