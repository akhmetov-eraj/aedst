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

    /// Only failures where the same request might succeed later. The offline
    /// cache fallback in Features/Profile keys off this.
    [[nodiscard]] constexpr bool isRetryable() const noexcept {
        return kind == ApiErrorKind::Transport || kind == ApiErrorKind::Timeout || kind == ApiErrorKind::Unavailable;
    }

    [[nodiscard]] core::Error toCoreError() const {
        switch (kind) {
        case ApiErrorKind::Transport:
        case ApiErrorKind::Timeout:
        case ApiErrorKind::Unavailable:
            return core::Error::network(message);
        case ApiErrorKind::Unauthenticated:
        case ApiErrorKind::PermissionDenied:
            return core::Error::unauthenticated(message);
        case ApiErrorKind::NotFound:
            return core::Error::notFound(message);
        case ApiErrorKind::InvalidArgument:
        case ApiErrorKind::AlreadyExists:
            return core::Error::invalidArgument(message);
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
