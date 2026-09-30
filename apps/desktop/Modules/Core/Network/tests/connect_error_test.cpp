#include "aedst/network/connect_error.hpp"

#include <gtest/gtest.h>

namespace aedst::network {
namespace {

TEST(ConnectErrorTest, ParsesTheSpecErrorBody) {
    const QByteArray body = R"({"code":"not_found","message":"task missing","details":[]})";

    const ApiError error = errorFromUnaryResponse(404, body, "HTTP fallback");

    EXPECT_EQ(error.kind, ApiErrorKind::NotFound);
    EXPECT_EQ(error.connectCode, "not_found");
    EXPECT_EQ(error.message, "task missing");
    EXPECT_EQ(error.httpStatus, 404);
}

TEST(ConnectErrorTest, CodeInBodyWinsOverHttpStatus) {
    const QByteArray body = R"({"code":"unauthenticated","message":"bad token"})";

    EXPECT_EQ(errorFromUnaryResponse(500, body, "").kind, ApiErrorKind::Unauthenticated);
}

TEST(ConnectErrorTest, NonJsonBodyFallsBackToTheHttpStatusTable) {
    const ApiError error = errorFromUnaryResponse(502, "<html>Bad Gateway</html>", "Bad Gateway");

    EXPECT_EQ(error.kind, ApiErrorKind::Unavailable);
    EXPECT_EQ(error.message, "Bad Gateway");
    EXPECT_TRUE(error.connectCode.empty());
}

TEST(ConnectErrorTest, EmptyBodyAndFallbackStillProducesAMessage) {
    EXPECT_EQ(errorFromUnaryResponse(503, "", "").message, "HTTP 503");
}

TEST(ConnectErrorTest, MissingMessageFallsBackToTheCode) {
    EXPECT_EQ(errorFromUnaryResponse(400, R"({"code":"invalid_argument"})", "").message, "invalid_argument");
}

TEST(EndStreamTest, EmptyAndErrorlessPayloadsAreCleanEnds) {
    EXPECT_TRUE(parseEndStream("").has_value());
    EXPECT_TRUE(parseEndStream("{}").has_value());
    EXPECT_TRUE(parseEndStream(R"({"metadata":{"x":["1"]}})").has_value());
}

TEST(EndStreamTest, EmbeddedErrorIsSurfaced) {
    const auto result = parseEndStream(R"({"error":{"code":"unavailable","message":"shutting down"}})");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, ApiErrorKind::Unavailable);
    EXPECT_EQ(result.error().message, "shutting down");
}

TEST(EndStreamTest, GarbageIsADecodingError) {
    const auto result = parseEndStream("not json");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, ApiErrorKind::Decoding);
}

} // namespace
} // namespace aedst::network
