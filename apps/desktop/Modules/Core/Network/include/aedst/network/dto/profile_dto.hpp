#pragma once

#include "aedst/network/dto/login_dto.hpp"

#include <QJsonObject>
#include <QString>

#include <optional>
#include <string>

/// Hand-mirrored from apps/server/api/proto/aedst/profile/v1/profile.proto.
namespace aedst::network::dto {

struct ProfileDto {
    std::string userId;
    std::string displayName;
    std::string bio;
    std::string avatarUrl;
    /// RFC 3339 (google.protobuf.Timestamp JSON form).
    std::string updatedAt;
};

struct GetProfileRequestDto {
    std::string userId;
};

struct UpdateProfileRequestDto {
    std::string userId;
    std::string displayName;
    std::string bio;
    std::string avatarUrl;
};

[[nodiscard]] inline QJsonObject toJson(const GetProfileRequestDto& dto) {
    return QJsonObject{{QStringLiteral("userId"), QString::fromStdString(dto.userId)}};
}

[[nodiscard]] inline QJsonObject toJson(const UpdateProfileRequestDto& dto) {
    return QJsonObject{
        {QStringLiteral("userId"), QString::fromStdString(dto.userId)},
        {QStringLiteral("displayName"), QString::fromStdString(dto.displayName)},
        {QStringLiteral("bio"), QString::fromStdString(dto.bio)},
        {QStringLiteral("avatarUrl"), QString::fromStdString(dto.avatarUrl)},
    };
}

[[nodiscard]] inline std::optional<ProfileDto> profileFromJson(const QJsonObject& json) {
    ProfileDto dto{
        .userId = detail::readString(json, QStringLiteral("userId")),
        .displayName = detail::readString(json, QStringLiteral("displayName")),
        .bio = detail::readString(json, QStringLiteral("bio")),
        .avatarUrl = detail::readString(json, QStringLiteral("avatarUrl")),
        .updatedAt = detail::readString(json, QStringLiteral("updatedAt")),
    };
    if (dto.userId.empty()) {
        return std::nullopt;
    }
    return dto;
}

[[nodiscard]] inline std::optional<ProfileDto> getProfileResponseFromJson(const QJsonObject& json) {
    return profileFromJson(json.value(QStringLiteral("profile")).toObject());
}

} // namespace aedst::network::dto
