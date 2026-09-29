#include "aedst/network/api_error.hpp"
#include "aedst/network/auth_interceptor.hpp"
#include "aedst/network/endpoints.hpp"
#include "aedst/network/dto/login_dto.hpp"

#include <QJsonObject>
#include <gtest/gtest.h>

namespace aedst::network {
namespace {

TEST(ApiErrorTest, MapsConnectCodesToKinds) {
    EXPECT_EQ(ApiError::kindFromConnectCode("unauthenticated"), ApiErrorKind::Unauthenticated);
    EXPECT_EQ(ApiError::kindFromConnectCode("not_found"), ApiErrorKind::NotFound);
    EXPECT_EQ(ApiError::kindFromConnectCode("invalid_argument"), ApiErrorKind::InvalidArgument);
    EXPECT_EQ(ApiError::kindFromConnectCode("unavailable"), ApiErrorKind::Unavailable);
    EXPECT_EQ(ApiError::kindFromConnectCode("deadline_exceeded"), ApiErrorKind::Timeout);
    EXPECT_EQ(ApiError::kindFromConnectCode("something_new"), ApiErrorKind::Unknown);
}

TEST(ApiErrorTest, FallsBackToHttpStatusWhenBodyHasNoCode) {
    const auto error = ApiError::fromConnectResponse(503, "", "upstream down");

    EXPECT_EQ(error.kind, ApiErrorKind::Unavailable);
    EXPECT_EQ(error.httpStatus, 503);
    EXPECT_EQ(error.message, "upstream down");
}

TEST(ApiErrorTest, ConnectCodeWinsOverHttpStatus) {
    const auto error = ApiError::fromConnectResponse(400, "not_found", "no such user");

    EXPECT_EQ(error.kind, ApiErrorKind::NotFound);
}

TEST(ApiErrorTest, OnlyTransientFailuresAreRetryable) {
    EXPECT_TRUE((ApiError{ApiErrorKind::Transport, 0, ""}).isRetryable());
    EXPECT_TRUE((ApiError{ApiErrorKind::Timeout, 0, ""}).isRetryable());
    EXPECT_TRUE((ApiError{ApiErrorKind::Unavailable, 503, ""}).isRetryable());
    EXPECT_FALSE((ApiError{ApiErrorKind::Unauthenticated, 401, ""}).isRetryable());
    EXPECT_FALSE((ApiError{ApiErrorKind::NotFound, 404, ""}).isRetryable());
}

TEST(ApiErrorTest, ConvertsToCoreErrorCodes) {
    EXPECT_EQ((ApiError{ApiErrorKind::Transport, 0, "offline"}).toCoreError(), core::Error::network("offline"));
    EXPECT_EQ((ApiError{ApiErrorKind::PermissionDenied, 403, "no"}).toCoreError().code,
              core::ErrorCode::Unauthenticated);
    EXPECT_EQ((ApiError{ApiErrorKind::Decoding, 200, "bad"}).toCoreError().code, core::ErrorCode::Unknown);
}

TEST(AuthInterceptorTest, LoginAndRefreshArePublic) {
    EXPECT_TRUE(AuthInterceptor::isPublicProcedure(endpoints::kAuthLogin));
    EXPECT_TRUE(AuthInterceptor::isPublicProcedure(endpoints::kAuthRefreshToken));
    EXPECT_FALSE(AuthInterceptor::isPublicProcedure(endpoints::kAuthLogout));
    EXPECT_FALSE(AuthInterceptor::isPublicProcedure(endpoints::kProfileGetProfile));
}

TEST(LoginDtoTest, DecodesProto3JsonWithStringInt64) {
    const QJsonObject json{
        {QStringLiteral("userId"), QStringLiteral("0b6c1c9e-0000-0000-0000-000000000001")},
        {QStringLiteral("tokens"),
         QJsonObject{
             {QStringLiteral("accessToken"), QStringLiteral("access")},
             {QStringLiteral("refreshToken"), QStringLiteral("refresh")},
             {QStringLiteral("accessTokenExpiresInSeconds"), QStringLiteral("900")},
             {QStringLiteral("refreshTokenExpiresInSeconds"), 2592000},
         }},
    };

    const auto dto = dto::loginResponseFromJson(json);

    ASSERT_TRUE(dto.has_value());
    EXPECT_EQ(dto->tokens.accessToken, "access");
    EXPECT_EQ(dto->tokens.accessTokenExpiresInSeconds, 900);
    EXPECT_EQ(dto->tokens.refreshTokenExpiresInSeconds, 2592000);
}

TEST(LoginDtoTest, RejectsResponseWithoutTokens) {
    const QJsonObject json{{QStringLiteral("userId"), QStringLiteral("u1")}};

    EXPECT_FALSE(dto::loginResponseFromJson(json).has_value());
}

} // namespace
} // namespace aedst::network
