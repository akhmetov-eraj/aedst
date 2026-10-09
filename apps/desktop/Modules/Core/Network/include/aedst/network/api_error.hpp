#pragma once

#include "aedst/core/error.hpp"

#include <expected>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

namespace aedst::network {

/// Mirrors the Connect protocol's error codes (https://connectrpc.com/docs/protocol#error-codes),
/// collapsed to what a client can act on differently.
enum class ApiErrorKind {
    Transport,
    Timeout,
    Cancelled,
    InvalidArgument,
    Unauthenticated,
    PermissionDenied,
    NotFound,
    AlreadyExists,
    Unavailable,
    Internal,
    Decoding,
    Unknown,
};

struct ApiError {
    ApiErrorKind kind{ApiErrorKind::Unknown};
    int httpStatus{0};
    std::string message;
    /// The Connect error code exactly as received ("unavailable", "not_found", ...);
    /// empty for client-side failures (transport, timeout, decoding) and for
    /// responses that carried no Connect error body.
    std::string connectCode{};
    /// `reason` of the google.rpc.ErrorInfo detail the server attached, if any (for example
    /// "CLIENT_UPGRADE_REQUIRED"); empty otherwise.
    std::string reason{};

    /// google.rpc.ErrorInfo reason of the server's client-version gate (apps/server/internal/platform/httpserver/
    /// clientversion.go, ADR-0009).
    static constexpr std::string_view kUpgradeRequiredReason = "CLIENT_UPGRADE_REQUIRED";

    /// The server refuses this app version: Connect code failed_precondition AND the CLIENT_UPGRADE_REQUIRED
    /// reason. The code alone is not enough, failed_precondition is also an ordinary business error.
    [[nodiscard]] bool isUpgradeRequired() const noexcept {
        return connectCode == "failed_precondition" && reason == kUpgradeRequiredReason;
    }

    [[nodiscard]] static constexpr ApiErrorKind kindFromConnectCode(std::string_view code) noexcept {
        if (code == "canceled") {
            return ApiErrorKind::Cancelled;
        }
        if (code == "invalid_argument" || code == "failed_precondition" || code == "out_of_range") {
            return ApiErrorKind::InvalidArgument;
        }
        if (code == "deadline_exceeded") {
            return ApiErrorKind::Timeout;
        }
        if (code == "not_found") {
            return ApiErrorKind::NotFound;
        }
        if (code == "already_exists" || code == "aborted") {
            return ApiErrorKind::AlreadyExists;
        }
        if (code == "permission_denied") {
            return ApiErrorKind::PermissionDenied;
        }
        if (code == "unauthenticated") {
            return ApiErrorKind::Unauthenticated;
        }
        if (code == "unavailable" || code == "resource_exhausted") {
            return ApiErrorKind::Unavailable;
        }
        if (code == "internal" || code == "data_loss" || code == "unimplemented") {
            return ApiErrorKind::Internal;
        }
        return ApiErrorKind::Unknown;
    }

    /// Fallback for responses that carry no Connect error body (proxies, LB errors).
    /// This is the HTTP-to-code table from the Connect protocol spec
    /// (https://connectrpc.com/docs/protocol#http-to-error-code) - note 400 maps to
    /// `internal` and 404 to `unimplemented`, not to invalid_argument/not_found:
    /// a bodiless 4xx came from infrastructure, not from the handler.
    [[nodiscard]] static constexpr ApiErrorKind kindFromHttpStatus(int status) noexcept {
        switch (status) {
        case 400:
            return ApiErrorKind::Internal;
        case 401:
            return ApiErrorKind::Unauthenticated;
        case 403:
            return ApiErrorKind::PermissionDenied;
        case 404:
            return ApiErrorKind::Internal; // unimplemented
        case 429:
        case 502:
        case 503:
        case 504:
            return ApiErrorKind::Unavailable;
        default:
            break;
        }
        return ApiErrorKind::Unknown;
    }

    [[nodiscard]] static ApiError fromConnectResponse(int httpStatus, std::string_view connectCode,
                                                      std::string message) {
        const ApiErrorKind kind =
            connectCode.empty() ? kindFromHttpStatus(httpStatus) : kindFromConnectCode(connectCode);
        return {.kind = kind,
                .httpStatus = httpStatus,
                .message = std::move(message),
                .connectCode = std::string(connectCode)};
    }

    /// Only failures where the same request might succeed later: `unavailable` (also 502/503/504 and 429
    /// without a Connect body), `deadline_exceeded` and connection-level failures, which Connect defines
    /// as `unavailable` on the client. An explicit `resource_exhausted` is NOT retried blindly: the
    /// server is shedding load on purpose and an immediate replay makes it worse. Background polling
    /// loops (Features/Notifications) back off on these instead of surfacing them.
    [[nodiscard]] constexpr bool isRetryable() const noexcept {
        if (connectCode == "resource_exhausted") {
            return false;
        }
        return kind == ApiErrorKind::Transport || kind == ApiErrorKind::Timeout || kind == ApiErrorKind::Unavailable;
    }

    /// The shared error kind of this failure (docs/contract/behavior/error-kinds.json). A connection-level
    /// failure (Transport) is reported as Offline: Qt does not tell "no network" from "server not reachable".
    [[nodiscard]] core::Error toCoreError() const {
        if (isUpgradeRequired()) {
            return core::Error::upgradeRequired(message);
        }
        switch (kind) {
        case ApiErrorKind::Transport:
            return core::Error::offline(message);
        case ApiErrorKind::Timeout:
            return core::Error::timeout(message);
        case ApiErrorKind::Unavailable:
            // kindFromConnectCode folds resource_exhausted into Unavailable (it is not retried blindly,
            // see isRetryable); as a kind it is a rate limit.
            return connectCode == "resource_exhausted" ? core::Error::rateLimited(message)
                                                       : core::Error::unavailable(message);
        case ApiErrorKind::Unauthenticated:
            return core::Error::unauthenticated(message);
        case ApiErrorKind::PermissionDenied:
            // Not "signed out": the session is valid but lacks a role (e.g. admin RPCs).
            return core::Error::forbidden(message);
        case ApiErrorKind::NotFound:
            return core::Error::notFound(message);
        case ApiErrorKind::InvalidArgument:
            return core::Error::invalidInput(message);
        case ApiErrorKind::AlreadyExists:
            return core::Error::conflict(message);
        case ApiErrorKind::Cancelled:
            return core::Error::cancelled(message);
        case ApiErrorKind::Internal:
        case ApiErrorKind::Decoding:
        case ApiErrorKind::Unknown:
            break;
        }
        return core::Error::unknown(message);
    }

    friend bool operator==(const ApiError&, const ApiError&) = default;
};

template <typename T>
using ApiResult = std::expected<T, ApiError>;

template <typename T>
using ApiCompletion = std::function<void(ApiResult<T>)>;

} // namespace aedst::network
