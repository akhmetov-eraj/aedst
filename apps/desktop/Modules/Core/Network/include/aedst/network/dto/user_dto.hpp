#pragma once

#include "aedst/network/dto/login_dto.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

/// Hand-mirrored from apps/server/api/proto/aedst/users/v1/users.proto.
namespace aedst::network::dto {

struct UserDto {
    std::string id;
    std::string email;
    std::vector<std::string> roles;
    /// RFC 3339 (google.protobuf.Timestamp JSON form).
    std::string createdAt;
};

struct GetUserRequestDto {
    std::string userId;
};

[[nodiscard]] inline QJsonObject toJson(const GetUserRequestDto& dto) {
    return QJsonObject{{QStringLiteral("userId"), QString::fromStdString(dto.userId)}};
}

[[nodiscard]] inline std::optional<UserDto> userFromJson(const QJsonObject& json) {
    UserDto dto{
        .id = detail::readString(json, QStringLiteral("id")),
        .email = detail::readString(json, QStringLiteral("email")),
        .roles = {},
        .createdAt = detail::readString(json, QStringLiteral("createdAt")),
    };
    if (dto.id.empty()) {
        return std::nullopt;
    }
    const QJsonArray roles = json.value(QStringLiteral("roles")).toArray();
    dto.roles.reserve(static_cast<std::size_t>(roles.size()));
    for (const QJsonValue& role : roles) {
        dto.roles.push_back(role.toString().toStdString());
    }
    return dto;
}

[[nodiscard]] inline std::optional<UserDto> getUserResponseFromJson(const QJsonObject& json) {
    return userFromJson(json.value(QStringLiteral("user")).toObject());
}

} // namespace aedst::network::dto
