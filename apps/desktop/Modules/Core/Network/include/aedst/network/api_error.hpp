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
    [[nodiscard]] static constexpr ApiErrorKind kindFromHttpStatus(int status) noexcept {
        switch (status) {
        case 400:
            return ApiErrorKind::InvalidArgument;
        case 401:
            return ApiErrorKind::Unauthenticated;
        case 403:
            return ApiErrorKind::PermissionDenied;
        case 404:
            return ApiErrorKind::NotFound;
        case 408:
            return ApiErrorKind::Timeout;
        case 409:
            return ApiErrorKind::AlreadyExists;
        case 429:
        case 502:
        case 503:
        case 504:
            return ApiErrorKind::Unavailable;
        default:
            break;
        }
        return status >= 500 ? ApiErrorKind::Internal : ApiErrorKind::Unknown;
    }

    [[nodiscard]] static ApiError fromConnectResponse(int httpStatus, std::string_view connectCode,
                                                      std::string message) {
        const ApiErrorKind kind =
            connectCode.empty() ? kindFromHttpStatus(httpStatus) : kindFromConnectCode(connectCode);
        return {kind, httpStatus, std::move(message)};
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
