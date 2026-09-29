#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

/// Hand-mirrored from apps/server/api/proto/aedst/auth/v1/auth.proto using the
/// proto3 JSON mapping (lowerCamelCase field names, int64 as JSON strings).
namespace aedst::network::dto {

struct TokenPairDto {
    std::string accessToken;
    std::string refreshToken;
    std::int64_t accessTokenExpiresInSeconds{0};
    std::int64_t refreshTokenExpiresInSeconds{0};
};

struct LoginRequestDto {
    std::string email;
    std::string password;
    std::string deviceId;
};

struct LoginResponseDto {
    std::string userId;
    TokenPairDto tokens;
};

struct RefreshTokenRequestDto {
    std::string refreshToken;
};

struct RefreshTokenResponseDto {
    TokenPairDto tokens;
};

struct LogoutRequestDto {
    std::string refreshToken;
};

namespace detail {

/// proto3 JSON encodes int64 as a string, but accept numbers too.
[[nodiscard]] inline std::int64_t readInt64(const QJsonValue& value) {
    if (value.isString()) {
        return value.toString().toLongLong();
    }
    return value.toInteger();
}

[[nodiscard]] inline std::string readString(const QJsonObject& json, const QString& key) {
    return json.value(key).toString().toStdString();
}

} // namespace detail

[[nodiscard]] inline QJsonObject toJson(const LoginRequestDto& dto) {
    return QJsonObject{
        {QStringLiteral("email"), QString::fromStdString(dto.email)},
        {QStringLiteral("password"), QString::fromStdString(dto.password)},
        {QStringLiteral("deviceId"), QString::fromStdString(dto.deviceId)},
    };
}

[[nodiscard]] inline QJsonObject toJson(const RefreshTokenRequestDto& dto) {
    return QJsonObject{{QStringLiteral("refreshToken"), QString::fromStdString(dto.refreshToken)}};
}

[[nodiscard]] inline QJsonObject toJson(const LogoutRequestDto& dto) {
    return QJsonObject{{QStringLiteral("refreshToken"), QString::fromStdString(dto.refreshToken)}};
}

[[nodiscard]] inline std::optional<TokenPairDto> tokenPairFromJson(const QJsonObject& json) {
    TokenPairDto dto{
        .accessToken = detail::readString(json, QStringLiteral("accessToken")),
        .refreshToken = detail::readString(json, QStringLiteral("refreshToken")),
        .accessTokenExpiresInSeconds = detail::readInt64(json.value(QStringLiteral("accessTokenExpiresInSeconds"))),
        .refreshTokenExpiresInSeconds =
            detail::readInt64(json.value(QStringLiteral("refreshTokenExpiresInSeconds"))),
    };
    if (dto.accessToken.empty() || dto.refreshToken.empty()) {
        return std::nullopt;
    }
    return dto;
}

[[nodiscard]] inline std::optional<LoginResponseDto> loginResponseFromJson(const QJsonObject& json) {
    auto tokens = tokenPairFromJson(json.value(QStringLiteral("tokens")).toObject());
    std::string userId = detail::readString(json, QStringLiteral("userId"));
    if (!tokens || userId.empty()) {
        return std::nullopt;
    }
    return LoginResponseDto{.userId = std::move(userId), .tokens = std::move(*tokens)};
}

[[nodiscard]] inline std::optional<RefreshTokenResponseDto> refreshTokenResponseFromJson(const QJsonObject& json) {
    auto tokens = tokenPairFromJson(json.value(QStringLiteral("tokens")).toObject());
    if (!tokens) {
        return std::nullopt;
    }
    return RefreshTokenResponseDto{.tokens = std::move(*tokens)};
}

} // namespace aedst::network::dto
