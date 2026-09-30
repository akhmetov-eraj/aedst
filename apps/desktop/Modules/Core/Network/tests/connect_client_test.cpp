#include "aedst/contract/methods.hpp"
#include "aedst/network/auth_interceptor.hpp"
#include "aedst/network/connect_client.hpp"
#include "aedst/network/connect_envelope.hpp"
#include "aedst/testing/fake_http_server.hpp"
#include "aedst/testing/stub_token_provider.hpp"
#include "aedst/testing/test_main.hpp"

#include <QByteArray>
#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// End-to-end tests of ConnectClient: real QNetworkAccessManager against the
// loopback FakeHttpServer, with the real generated messages from Aedst::Contract.

namespace aedst::network {
namespace {

using namespace std::chrono_literals;
namespace authv1 = ::aedst::auth::v1;
using testing::FakeHttpServer;
using testing::FakeResponse;
using testing::waitUntil;

QByteArray serialized(const google::protobuf::MessageLite& message) {
    return QByteArray::fromStdString(message.SerializeAsString());
}

authv1::LoginRequest loginRequest() {
    authv1::LoginRequest request;
    request.set_email("a@b.co");
    request.set_password("x");
    request.set_device_id("d");
    return request;
}

QByteArray loginResponseBytes(const std::string& userId = "u1") {
    authv1::LoginResponse response;
    response.set_user_id(userId);
    response.mutable_tokens()->set_access_token("access");
    response.mutable_tokens()->set_refresh_token("refresh");
    response.mutable_tokens()->set_access_token_expires_in_seconds(900);
    return serialized(response);
}

QByteArray currentUserResponseBytes() {
    authv1::GetCurrentUserResponse response;
    response.mutable_user()->set_id("u1");
    response.mutable_user()->set_email("a@b.co");
    return serialized(response);
}

RetryPolicy fastRetry() {
    return RetryPolicy{.maxAttempts = 3, .initialBackoff = 1ms, .maxBackoff = 2ms, .multiplier = 2.0};
}

class ConnectClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_TRUE(server_.isListening());
        client_ = std::make_unique<ConnectClient>(
            ClientConfig{.baseUrl = server_.baseUrl(), .defaultTimeout = 2s, .retry = fastRetry()}, interceptor_);
    }

    template <typename Res>
    struct Outcome {
        std::optional<ApiResult<Res>> result;
        int completions{0};
        bool done() const { return result.has_value(); }
    };

    template <typename Req, typename Res>
    CallHandle call(const Method<Req, Res>& method, const Req& request, Outcome<Res>& out,
                    const CallOptions& options = {}) {
        return client_->unary(
            method, request,
            [&out](ApiResult<Res> result) {
                out.result = std::move(result);
                ++out.completions;
            },
            options);
    }

    FakeHttpServer server_;
    std::shared_ptr<testing::StubTokenProvider> tokens_ = std::make_shared<testing::StubTokenProvider>("tok");
    std::shared_ptr<AuthInterceptor> interceptor_ = std::make_shared<AuthInterceptor>(tokens_);
    std::unique_ptr<ConnectClient> client_;
};

TEST_F(ConnectClientTest, UnarySendsTheConnectWireFormatAndDecodesTheResponse) {
    server_.enqueue(FakeResponse::proto(loginResponseBytes("user-42")));
    Outcome<authv1::LoginResponse> out;

    call(contract::kAuthLogin, loginRequest(), out);
    ASSERT_TRUE(waitUntil([&] { return out.done(); }));

    ASSERT_TRUE(out.result->has_value());
    EXPECT_EQ((*out.result)->user_id(), "user-42");
    EXPECT_EQ((*out.result)->tokens().access_token_expires_in_seconds(), 900);

    ASSERT_EQ(server_.requests().size(), 1U);
    const auto& request = server_.requests().front();
    EXPECT_EQ(request.method, QByteArray("POST"));
    EXPECT_EQ(request.target, QByteArray("/aedst.auth.v1.AuthService/Login"));
    EXPECT_EQ(request.headers.value("content-type"), QByteArray("application/proto"));
    EXPECT_EQ(request.headers.value("connect-protocol-version"), QByteArray("1"));
    EXPECT_EQ(request.headers.value("connect-timeout-ms"), QByteArray("2000"));
    // Golden bytes: email=1, password=2, device_id=3, all length-delimited strings.
    EXPECT_EQ(request.body, QByteArray::fromHex("0a06" "6140622e636f" "1201" "78" "1a01" "64"));
}

TEST_F(ConnectClientTest, BearerTokenIsSentOnlyToAuthenticatedMethods) {
    server_.enqueue(FakeResponse::proto(currentUserResponseBytes()));
    server_.enqueue(FakeResponse::proto(loginResponseBytes()));
    Outcome<authv1::GetCurrentUserResponse> me;
    Outcome<authv1::LoginResponse> login;

    call(contract::kAuthGetCurrentUser, authv1::GetCurrentUserRequest{}, me);
    ASSERT_TRUE(waitUntil([&] { return me.done(); }));
    call(contract::kAuthLogin, loginRequest(), login);
    ASSERT_TRUE(waitUntil([&] { return login.done(); }));

    ASSERT_EQ(server_.requests().size(), 2U);
    EXPECT_EQ(server_.requests()[0].headers.value("authorization"), QByteArray("Bearer tok"));
    EXPECT_FALSE(server_.requests()[1].headers.contains("authorization"));
}

TEST_F(ConnectClientTest, ConnectErrorBodyBecomesATypedError) {
    server_.enqueue(FakeResponse::connectError(401, "unauthenticated", "invalid credentials"));
    Outcome<authv1::LoginResponse> out;

    call(contract::kAuthLogin, loginRequest(), out);
    ASSERT_TRUE(waitUntil([&] { return out.done(); }));

    ASSERT_FALSE(out.result->has_value());
    const ApiError& error = out.result->error();
    EXPECT_EQ(error.kind, ApiErrorKind::Unauthenticated);
    EXPECT_EQ(error.connectCode, "unauthenticated");
    EXPECT_EQ(error.message, "invalid credentials");
    EXPECT_EQ(error.httpStatus, 401);
}

TEST_F(ConnectClientTest, IdempotentCallsRetryTransientFailuresWithBackoff) {
    server_.enqueue(FakeResponse::connectError(503, "unavailable", "warming up"));
    server_.enqueue(FakeResponse::dropped());
    server_.enqueue(FakeResponse::proto(currentUserResponseBytes()));
    Outcome<authv1::GetCurrentUserResponse> out;

    call(contract::kAuthGetCurrentUser, authv1::GetCurrentUserRequest{}, out);
    ASSERT_TRUE(waitUntil([&] { return out.done(); }));

    ASSERT_TRUE(out.result->has_value());
    EXPECT_EQ((*out.result)->user().id(), "u1");
    EXPECT_EQ(server_.requests().size(), 3U);
    EXPECT_EQ(out.completions, 1);
}

TEST_F(ConnectClientTest, RetriesStopAtMaxAttemptsAndReportTheLastError) {
    for (int i = 0; i < 5; ++i) {
        server_.enqueue(FakeResponse::connectError(503, "unavailable", "still down"));
    }
    Outcome<authv1::GetCurrentUserResponse> out;

    call(contract::kAuthGetCurrentUser, authv1::GetCurrentUserRequest{}, out);
    ASSERT_TRUE(waitUntil([&] { return out.done(); }));

    ASSERT_FALSE(out.result->has_value());
    EXPECT_EQ(out.result->error().kind, ApiErrorKind::Unavailable);
    EXPECT_EQ(server_.requests().size(), 3U);
}

TEST_F(ConnectClientTest, NonIdempotentCallsAreNeverRetried) {
    server_.enqueue(FakeResponse::connectError(503, "unavailable", "down"));
    server_.enqueue(FakeResponse::proto(loginResponseBytes()));
    Outcome<authv1::LoginResponse> out;

    call(contract::kAuthLogin, loginRequest(), out);
    ASSERT_TRUE(waitUntil([&] { return out.done(); }));

    ASSERT_FALSE(out.result->has_value());
    EXPECT_EQ(server_.requests().size(), 1U);
}

TEST_F(ConnectClientTest, NonTransientErrorsAreNotRetriedEvenWhenIdempotent) {
    server_.enqueue(FakeResponse::connectError(401, "unauthenticated", "expired"));
    server_.enqueue(FakeResponse::proto(currentUserResponseBytes()));
    Outcome<authv1::GetCurrentUserResponse> out;

    call(contract::kAuthGetCurrentUser, authv1::GetCurrentUserRequest{}, out);
    ASSERT_TRUE(waitUntil([&] { return out.done(); }));

    ASSERT_FALSE(out.result->has_value());
    EXPECT_EQ(out.result->error().kind, ApiErrorKind::Unauthenticated);
    EXPECT_EQ(server_.requests().size(), 1U);
}

TEST_F(ConnectClientTest, ConnectionDropIsATransportError) {
    server_.enqueue(FakeResponse::dropped());
    Outcome<authv1::LoginResponse> out;

    call(contract::kAuthLogin, loginRequest(), out);
    ASSERT_TRUE(waitUntil([&] { return out.done(); }));

    ASSERT_FALSE(out.result->has_value());
    EXPECT_EQ(out.result->error().kind, ApiErrorKind::Transport);
}

TEST_F(ConnectClientTest, DeadlineExpiryIsATimeout) {
    FakeResponse slow = FakeResponse::proto(loginResponseBytes());
    slow.delay = 400ms;
    server_.enqueue(std::move(slow));
    Outcome<authv1::LoginResponse> out;

    call(contract::kAuthLogin, loginRequest(), out, CallOptions{.timeout = 50ms});
    ASSERT_TRUE(waitUntil([&] { return out.done(); }));

    ASSERT_FALSE(out.result->has_value());
    EXPECT_EQ(out.result->error().kind, ApiErrorKind::Timeout);
}

TEST_F(ConnectClientTest, CancelCompletesOnceWithCancelled) {
    FakeResponse slow = FakeResponse::proto(loginResponseBytes());
    slow.delay = 400ms;
    server_.enqueue(std::move(slow));
    Outcome<authv1::LoginResponse> out;

    const CallHandle handle = call(contract::kAuthLogin, loginRequest(), out);
    ASSERT_TRUE(waitUntil([&] { return !server_.requests().empty(); }));
    EXPECT_TRUE(handle.isActive());
    handle.cancel();

    ASSERT_TRUE(waitUntil([&] { return out.done(); }));
    ASSERT_FALSE(out.result->has_value());
    EXPECT_EQ(out.result->error().kind, ApiErrorKind::Cancelled);
    EXPECT_FALSE(handle.isActive());
    // Let the late server response arrive; it must not complete the call again.
    waitUntil([] { return false; }, 500ms);
    EXPECT_EQ(out.completions, 1);
}

TEST_F(ConnectClientTest, ANonProtobufSuccessResponseIsRejected) {
    FakeResponse portal;
    portal.contentType = "text/html";
    portal.body = "<html>captive portal</html>";
    server_.enqueue(std::move(portal));
    Outcome<authv1::LoginResponse> out;

    call(contract::kAuthLogin, loginRequest(), out);
    ASSERT_TRUE(waitUntil([&] { return out.done(); }));

    ASSERT_FALSE(out.result->has_value());
    EXPECT_EQ(out.result->error().kind, ApiErrorKind::Decoding);
}

TEST_F(ConnectClientTest, MalformedProtobufIsADecodingError) {
    server_.enqueue(FakeResponse::proto(QByteArray::fromHex("ffffff")));
    Outcome<authv1::LoginResponse> out;

    call(contract::kAuthLogin, loginRequest(), out);
    ASSERT_TRUE(waitUntil([&] { return out.done(); }));

    ASSERT_FALSE(out.result->has_value());
    EXPECT_EQ(out.result->error().kind, ApiErrorKind::Decoding);
}

// --- server streaming -----------------------------------------------------------

constexpr auto kStreamMethod =
    makeMethod<authv1::LoginRequest, authv1::LoginResponse>("/test.v1.Stream/Watch", AuthRequirement::Required);

struct StreamOutcome {
    std::vector<std::string> userIds;
    std::optional<ApiResult<void>> done;
};

TEST_F(ConnectClientTest, ServerStreamDeliversMessagesAcrossArbitraryChunkBoundaries) {
    const QByteArray wire = envelope::encode(0, loginResponseBytes("m1")) + envelope::encode(0, loginResponseBytes("m2")) +
                            envelope::encode(envelope::kFlagEndStream, "{}");
    server_.enqueue(FakeResponse::connectStream({wire.left(3), wire.mid(3, 20), wire.mid(23)}));
    StreamOutcome out;

    client_->serverStream(
        kStreamMethod, loginRequest(),
        [&out](authv1::LoginResponse message) { out.userIds.push_back(message.user_id()); },
        [&out](ApiResult<void> result) { out.done = std::move(result); });
    ASSERT_TRUE(waitUntil([&] { return out.done.has_value(); }));

    EXPECT_TRUE(out.done->has_value());
    EXPECT_EQ(out.userIds, (std::vector<std::string>{"m1", "m2"}));
    ASSERT_EQ(server_.requests().size(), 1U);
    const auto& request = server_.requests().front();
    EXPECT_EQ(request.headers.value("content-type"), QByteArray("application/connect+proto"));
    EXPECT_EQ(request.body, envelope::encode(0, serialized(loginRequest())));
}

TEST_F(ConnectClientTest, ServerStreamSurfacesAnErrorInTheEndOfStreamFrame) {
    const QByteArray wire = envelope::encode(0, loginResponseBytes("m1")) +
                            envelope::encode(envelope::kFlagEndStream,
                                             R"({"error":{"code":"unavailable","message":"draining"}})");
    server_.enqueue(FakeResponse::connectStream({wire}));
    StreamOutcome out;

    client_->serverStream(
        kStreamMethod, loginRequest(),
        [&out](authv1::LoginResponse message) { out.userIds.push_back(message.user_id()); },
        [&out](ApiResult<void> result) { out.done = std::move(result); });
    ASSERT_TRUE(waitUntil([&] { return out.done.has_value(); }));

    ASSERT_FALSE(out.done->has_value());
    EXPECT_EQ(out.done->error().kind, ApiErrorKind::Unavailable);
    EXPECT_EQ(out.done->error().message, "draining");
    EXPECT_EQ(out.userIds.size(), 1U);
}

TEST_F(ConnectClientTest, ServerStreamCutBeforeEndOfStreamIsATransportError) {
    server_.enqueue(FakeResponse::connectStream({envelope::encode(0, loginResponseBytes("m1"))}));
    StreamOutcome out;

    client_->serverStream(
        kStreamMethod, loginRequest(),
        [&out](authv1::LoginResponse message) { out.userIds.push_back(message.user_id()); },
        [&out](ApiResult<void> result) { out.done = std::move(result); });
    ASSERT_TRUE(waitUntil([&] { return out.done.has_value(); }));

    ASSERT_FALSE(out.done->has_value());
    EXPECT_EQ(out.done->error().kind, ApiErrorKind::Transport);
}

TEST_F(ConnectClientTest, ServerStreamHttpErrorUsesTheUnaryErrorBody) {
    server_.enqueue(FakeResponse::connectError(403, "permission_denied", "nope"));
    StreamOutcome out;

    client_->serverStream(
        kStreamMethod, loginRequest(), [&out](authv1::LoginResponse) {},
        [&out](ApiResult<void> result) { out.done = std::move(result); });
    ASSERT_TRUE(waitUntil([&] { return out.done.has_value(); }));

    ASSERT_FALSE(out.done->has_value());
    EXPECT_EQ(out.done->error().kind, ApiErrorKind::PermissionDenied);
}

} // namespace
} // namespace aedst::network

AEDST_GTEST_MAIN_WITH_QCOREAPPLICATION()
