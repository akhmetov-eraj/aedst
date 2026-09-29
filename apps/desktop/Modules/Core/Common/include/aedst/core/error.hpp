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

    [[nodiscard]] static Error invalidArgument(std::string message) {
        return {ErrorCode::InvalidArgument, std::move(message)};
    }

    [[nodiscard]] static Error unauthenticated(std::string message) {
        return {ErrorCode::Unauthenticated, std::move(message)};
    }

    [[nodiscard]] static Error notFound(std::string message) { return {ErrorCode::NotFound, std::move(message)}; }

    [[nodiscard]] static Error network(std::string message) { return {ErrorCode::Network, std::move(message)}; }

    [[nodiscard]] static Error storage(std::string message) { return {ErrorCode::Storage, std::move(message)}; }

    [[nodiscard]] static Error cancelled(std::string message) { return {ErrorCode::Cancelled, std::move(message)}; }

    [[nodiscard]] static Error unknown(std::string message) { return {ErrorCode::Unknown, std::move(message)}; }

    friend bool operator==(const Error&, const Error&) = default;
};

} // namespace aedst::core
