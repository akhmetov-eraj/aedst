#pragma once

#include <string>
#include <string_view>
#include <utility>

namespace aedst::core {

/// Transport-agnostic error categories. Network/persistence errors are mapped
/// into these at the Data layer so Domain and Presentation never see
/// QNetworkReply or sqlite3 codes.
enum class ErrorCode {
    Unknown,
    InvalidArgument,
    Unauthenticated,
    /// Signed in, but the server refused the call for this identity (missing role).
    /// Distinct from Unauthenticated so a forbidden admin call never signs the user out.
    PermissionDenied,
    NotFound,
    Network,
    Storage,
    Cancelled,
};

[[nodiscard]] constexpr std::string_view toString(ErrorCode code) noexcept {
    switch (code) {
    case ErrorCode::InvalidArgument:
        return "invalid_argument";
    case ErrorCode::Unauthenticated:
        return "unauthenticated";
    case ErrorCode::PermissionDenied:
        return "permission_denied";
    case ErrorCode::NotFound:
        return "not_found";
    case ErrorCode::Network:
        return "network";
    case ErrorCode::Storage:
        return "storage";
    case ErrorCode::Cancelled:
        return "cancelled";
    case ErrorCode::Unknown:
        break;
    }
    return "unknown";
}

struct Error {
    ErrorCode code{ErrorCode::Unknown};
    std::string message;

    [[nodiscard]] static Error invalidArgument(std::string text) {
        return {ErrorCode::InvalidArgument, std::move(text)};
    }

    [[nodiscard]] static Error unauthenticated(std::string text) {
        return {ErrorCode::Unauthenticated, std::move(text)};
    }

    [[nodiscard]] static Error permissionDenied(std::string text) {
        return {ErrorCode::PermissionDenied, std::move(text)};
    }

    [[nodiscard]] static Error notFound(std::string text) { return {ErrorCode::NotFound, std::move(text)}; }

    [[nodiscard]] static Error network(std::string text) { return {ErrorCode::Network, std::move(text)}; }

    [[nodiscard]] static Error storage(std::string text) { return {ErrorCode::Storage, std::move(text)}; }

    [[nodiscard]] static Error cancelled(std::string text) { return {ErrorCode::Cancelled, std::move(text)}; }

    [[nodiscard]] static Error unknown(std::string text) { return {ErrorCode::Unknown, std::move(text)}; }

    friend bool operator==(const Error&, const Error&) = default;
};

} // namespace aedst::core
