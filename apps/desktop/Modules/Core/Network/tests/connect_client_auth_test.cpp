#include "aedst/contract/methods.hpp"
#include "aedst/network/auth_interceptor.hpp"
#include "aedst/network/connect_client.hpp"
#include "aedst/network/connect_envelope.hpp"
#include "aedst/testing/fake_http_server.hpp"

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// Token refresh inside ConnectClient (pre-flight and once after `unauthenticated`), cancellation tied to a
// caller's lifetime, and the guard that keeps a throwing completion from reaching the Qt event loop.
// main() lives in connect_client_test.cpp (same binary).

namespace aedst::network {
namespace {

using namespace std::chrono_literals;
namespace authv1 = ::aedst::auth::v1;
using testing::FakeHttpServer;
using testing::FakeResponse;
using testing::waitUntil;

QByteArray userResponse() {
    authv1::GetCurrentUserResponse response;
    response.mutable_user()->set_id("u1");
    return QByteArray::fromStdString(response.SerializeAsString());
}

QByteArray loginResponse(const std::string& userId) {
    authv1::LoginResponse response;
    response.set_user_id(userId);
    return QByteArray::fromStdString(response.SerializeAsString());
}

constexpr auto kStream =
    makeMethod<authv1::LoginRequest, authv1::LoginResponse>("/test.v1.Stream/Watch", AuthRequirement::Required);

/// Token source + refresher with scripted behaviour; completions may be delayed to model a network refresh.
class FakeSession final : public TokenProvider, public TokenRefresher {
public:
    [[nodiscard]] std::optional<std::string> accessToken() const override { return token; }

    void ensureFreshAccessToken(core::Completion<std::string> done) override {
        ++ensureCalls;
        if (ensureError) {
            deliver(std::move(done), core::fail(*ensureError));
            return;
        }
        deliver(std::move(done), token.value_or(std::string{}));
    }

    void refreshAfterRejection(const std::string& rejectedToken, core::Completion<std::string> done) override {
        ++rejectionCalls;
        lastRejected = rejectedToken;
        if (refreshError) {
            deliver(std::move(done), core::fail(*refreshError));
            return;
        }
        token = nextToken;
        deliver(std::move(done), *token);
    }

    std::optional<std::string> token{"old"};
    std::string nextToken{"new"};
    std::optional<core::Error> ensureError;
    std::optional<core::Error> refreshError;
    std::chrono::milliseconds refreshDelay{0};
    int ensureCalls{0};
    int rejectionCalls{0};
    std::string lastRejected;

private:
    void deliver(core::Completion<std::string> done, core::Outcome<std::string> result) const {
        if (refreshDelay.count() <= 0) {
            done(std::move(result));
            return;
        }
        QTimer::singleShot(refreshDelay, [done = std::move(done), result = std::move(result)]() mutable {
            done(std::move(result));
        });
    }
};

class ClientAuthTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_TRUE(server_.isListening());
        interceptor_ = std::make_shared<AuthInterceptor>(session_, session_);
        client_ = std::make_unique<ConnectClient>(
            ClientConfig{.baseUrl = server_.baseUrl(),
                         .defaultTimeout = 2s,
                         .retry = RetryPolicy{.maxAttempts = 3, .initialBackoff = 1ms, .maxBackoff = 2ms},
                         .allowInsecureLoopback = true},
            interceptor_);
    }

    template <typename Req, typename Res>
    CallHandle call(const Method<Req, Res>& method, const Req& request, std::optional<ApiResult<Res>>& out) {
        return client_->unary(method, request, [&out](ApiResult<Res> result) { out = std::move(result); });
    }

    void tasksOk() {
        ::aedst::tasks::v1::CreateTaskResponse response;
        server_.enqueue(FakeResponse::proto(QByteArray::fromStdString(response.SerializeAsString())));
    }

    [[nodiscard]] QByteArray bearer(std::size_t index) const {
        return server_.requests().at(index).headers.value("authorization");
    }

    FakeHttpServer server_;
    std::shared_ptr<FakeSession> session_ = std::make_shared<FakeSession>();
    std::shared_ptr<AuthInterceptor> interceptor_;
    std::unique_ptr<ConnectClient> client_;
};

// --- Pre-flight refresh ----------------------------------------------------------------------

TEST_F(ClientAuthTest, AuthenticatedCallsAskTheRefresherFirstAndSendTheFreshToken) {
    session_->token = "fresh";
    server_.enqueue(FakeResponse::proto(userResponse()));
    std::optional<ApiResult<authv1::GetCurrentUserResponse>> out;

    call(contract::kAuthGetCurrentUser, authv1::GetCurrentUserRequest{}, out);
    ASSERT_TRUE(waitUntil([&] { return out.has_value(); }));

    ASSERT_TRUE(out->has_value());
    EXPECT_EQ(session_->ensureCalls, 1);
    EXPECT_EQ(bearer(0), QByteArray("Bearer fresh"));
}

TEST_F(ClientAuthTest, PublicCallsNeverTouchTheRefresher) {
    server_.enqueue(FakeResponse::proto(loginResponse("u")));
    std::optional<ApiResult<authv1::LoginResponse>> out;

    call(contract::kAuthLogin, authv1::LoginRequest{}, out);
    ASSERT_TRUE(waitUntil([&] { return out.has_value(); }));

    EXPECT_EQ(session_->ensureCalls, 0);
    EXPECT_FALSE(server_.requests().front().headers.contains("authorization"));
}

TEST_F(ClientAuthTest, AnEndedSessionFailsTheCallAsynchronouslyWithoutSendingAnything) {
    session_->ensureError = core::Error::unauthenticated("Not signed in");
    std::optional<ApiResult<authv1::GetCurrentUserResponse>> out;

    call(contract::kAuthGetCurrentUser, authv1::GetCurrentUserRequest{}, out);
    EXPECT_FALSE(out.has_value()) << "completion must be asynchronous";
    ASSERT_TRUE(waitUntil([&] { return out.has_value(); }));

    ASSERT_FALSE(out->has_value());
    EXPECT_EQ(out->error().kind, ApiErrorKind::Unauthenticated);
    EXPECT_TRUE(server_.requests().empty());
}

TEST_F(ClientAuthTest, ATransientRefreshFailureStillSendsTheCurrentToken) {
    session_->ensureError = core::Error::network("offline");
    server_.enqueue(FakeResponse::proto(userResponse()));
    std::optional<ApiResult<authv1::GetCurrentUserResponse>> out;

    call(contract::kAuthGetCurrentUser, authv1::GetCurrentUserRequest{}, out);
    ASSERT_TRUE(waitUntil([&] { return out.has_value(); }));

    ASSERT_TRUE(out->has_value());
    EXPECT_EQ(bearer(0), QByteArray("Bearer old"));
}

TEST_F(ClientAuthTest, ADelayedPreflightRefreshHoldsTheRequestBack) {
    session_->refreshDelay = 80ms;
    server_.enqueue(FakeResponse::proto(userResponse()));
    std::optional<ApiResult<authv1::GetCurrentUserResponse>> out;

    call(contract::kAuthGetCurrentUser, authv1::GetCurrentUserRequest{}, out);
    waitUntil([] { return false; }, 20ms);
    EXPECT_TRUE(server_.requests().empty());

    ASSERT_TRUE(waitUntil([&] { return out.has_value(); }));
    EXPECT_TRUE(out->has_value());
}

// --- Refresh after a rejection ----------------------------------------------------------------

TEST_F(ClientAuthTest, UnauthenticatedResponseRefreshesOnceAndReplaysTheCall) {
    server_.enqueue(FakeResponse::connectError(401, "unauthenticated", "token expired"));
    server_.enqueue(FakeResponse::proto(userResponse()));
    std::optional<ApiResult<authv1::GetCurrentUserResponse>> out;

    call(contract::kAuthGetCurrentUser, authv1::GetCurrentUserRequest{}, out);
    ASSERT_TRUE(waitUntil([&] { return out.has_value(); }));

    ASSERT_TRUE(out->has_value());
    EXPECT_EQ(session_->rejectionCalls, 1);
    EXPECT_EQ(session_->lastRejected, "old");
    ASSERT_EQ(server_.requests().size(), 2U);
    EXPECT_EQ(bearer(0), QByteArray("Bearer old"));
    EXPECT_EQ(bearer(1), QByteArray("Bearer new"));
}

TEST_F(ClientAuthTest, NonIdempotentCallsAreReplayedAfterARejectionToo) {
    // The rejected request never reached the handler, so replaying a CreateTask cannot duplicate it.
    server_.enqueue(FakeResponse::connectError(401, "unauthenticated", "token expired"));
    tasksOk();
    std::optional<ApiResult<::aedst::tasks::v1::CreateTaskResponse>> out;

    call(contract::kTasksCreateTask, ::aedst::tasks::v1::CreateTaskRequest{}, out);
    ASSERT_TRUE(waitUntil([&] { return out.has_value(); }));

    EXPECT_TRUE(out->has_value());
    EXPECT_EQ(server_.requests().size(), 2U);
    EXPECT_EQ(session_->rejectionCalls, 1);
}

TEST_F(ClientAuthTest, ASecondRejectionIsFinal) {
    server_.enqueue(FakeResponse::connectError(401, "unauthenticated", "no"));
    server_.enqueue(FakeResponse::connectError(401, "unauthenticated", "still no"));
    std::optional<ApiResult<authv1::GetCurrentUserResponse>> out;

    call(contract::kAuthGetCurrentUser, authv1::GetCurrentUserRequest{}, out);
    ASSERT_TRUE(waitUntil([&] { return out.has_value(); }));

    ASSERT_FALSE(out->has_value());
    EXPECT_EQ(out->error().kind, ApiErrorKind::Unauthenticated);
    EXPECT_EQ(session_->rejectionCalls, 1) << "one refresh per call, never a loop";
    EXPECT_EQ(server_.requests().size(), 2U);
}

TEST_F(ClientAuthTest, ARefreshTokenThatIsRejectedEndsTheCallAsUnauthenticated) {
    session_->refreshError = core::Error::unauthenticated("refresh token revoked");
    server_.enqueue(FakeResponse::connectError(401, "unauthenticated", "no"));
    std::optional<ApiResult<authv1::GetCurrentUserResponse>> out;

    call(contract::kAuthGetCurrentUser, authv1::GetCurrentUserRequest{}, out);
    ASSERT_TRUE(waitUntil([&] { return out.has_value(); }));

    ASSERT_FALSE(out->has_value());
    EXPECT_EQ(out->error().kind, ApiErrorKind::Unauthenticated);
    EXPECT_EQ(server_.requests().size(), 1U);
}

TEST_F(ClientAuthTest, AFailedRefreshDueToTheNetworkIsNotReportedAsASignOut) {
    session_->refreshError = core::Error::network("offline");
    server_.enqueue(FakeResponse::connectError(401, "unauthenticated", "no"));
    std::optional<ApiResult<authv1::GetCurrentUserResponse>> out;

    call(contract::kAuthGetCurrentUser, authv1::GetCurrentUserRequest{}, out);
    ASSERT_TRUE(waitUntil([&] { return out.has_value(); }));

    ASSERT_FALSE(out->has_value());
    EXPECT_EQ(out->error().kind, ApiErrorKind::Transport) << "Unauthenticated here would sign the user out offline";
    EXPECT_TRUE(out->error().isRetryable());
}

TEST_F(ClientAuthTest, PublicCallsAreNeverRefreshedOrReplayed) {
    server_.enqueue(FakeResponse::connectError(401, "unauthenticated", "invalid credentials"));
    std::optional<ApiResult<authv1::LoginResponse>> out;

    call(contract::kAuthLogin, authv1::LoginRequest{}, out);
    ASSERT_TRUE(waitUntil([&] { return out.has_value(); }));

    ASSERT_FALSE(out->has_value());
    EXPECT_EQ(session_->rejectionCalls, 0);
    EXPECT_EQ(server_.requests().size(), 1U);
}

TEST_F(ClientAuthTest, TheRefreshReplayDoesNotConsumeTheRetryBudget) {
    server_.enqueue(FakeResponse::connectError(401, "unauthenticated", "expired"));
    server_.enqueue(FakeResponse::connectError(503, "unavailable", "busy"));
    server_.enqueue(FakeResponse::connectError(503, "unavailable", "busy"));
    server_.enqueue(FakeResponse::proto(userResponse()));
    std::optional<ApiResult<authv1::GetCurrentUserResponse>> out;

    call(contract::kAuthGetCurrentUser, authv1::GetCurrentUserRequest{}, out);
    ASSERT_TRUE(waitUntil([&] { return out.has_value(); }));

    EXPECT_TRUE(out->has_value()) << "401 + refresh, then two 503 retries still fit in maxAttempts=3";
    EXPECT_EQ(server_.requests().size(), 4U);
}

TEST_F(ClientAuthTest, CancellingWhileTheRefreshIsRunningNeitherReplaysNorLeaks) {
    session_->refreshDelay = 100ms;
    server_.enqueue(FakeResponse::connectError(401, "unauthenticated", "expired"));
    std::optional<ApiResult<authv1::GetCurrentUserResponse>> out;
    int completions = 0;
    auto handle = client_->unary(
        contract::kAuthGetCurrentUser, authv1::GetCurrentUserRequest{},
        [&](ApiResult<authv1::GetCurrentUserResponse> result) {
            out = std::move(result);
            ++completions;
        });
    ASSERT_TRUE(waitUntil([&] { return session_->rejectionCalls == 1; }));

    handle.cancel();
    ASSERT_TRUE(waitUntil([&] { return out.has_value(); }));
    waitUntil([] { return false; }, 200ms); // let the delayed refresh complete

    ASSERT_FALSE(out->has_value());
    EXPECT_EQ(out->error().kind, ApiErrorKind::Cancelled);
    EXPECT_EQ(completions, 1);
    EXPECT_EQ(server_.requests().size(), 1U) << "no replay after cancel";
}

// --- Server streams -------------------------------------------------------------------------------

TEST_F(ClientAuthTest, AStreamRejectedAtOpenRefreshesOnceAndReopens) {
    server_.enqueue(FakeResponse::connectError(401, "unauthenticated", "expired"));
    server_.enqueue(FakeResponse::connectStream(
        {envelope::encode(0, loginResponse("m1")) + envelope::encode(envelope::kFlagEndStream, "{}")}));
    std::vector<std::string> ids;
    std::optional<ApiResult<void>> done;

    client_->serverStream(
        kStream, authv1::LoginRequest{}, [&ids](authv1::LoginResponse message) { ids.push_back(message.user_id()); },
        [&done](ApiResult<void> result) { done = std::move(result); });
    ASSERT_TRUE(waitUntil([&] { return done.has_value(); }));

    EXPECT_TRUE(done->has_value());
    EXPECT_EQ(ids, (std::vector<std::string>{"m1"}));
    ASSERT_EQ(server_.requests().size(), 2U);
    EXPECT_EQ(bearer(0), QByteArray("Bearer old"));
    EXPECT_EQ(bearer(1), QByteArray("Bearer new"));
    EXPECT_EQ(session_->rejectionCalls, 1);
}

TEST_F(ClientAuthTest, AStreamWithAnEndedSessionNeverOpens) {
    session_->ensureError = core::Error::unauthenticated("Not signed in");
    std::optional<ApiResult<void>> done;

    client_->serverStream(
        kStream, authv1::LoginRequest{}, [](authv1::LoginResponse) {},
        [&done](ApiResult<void> result) { done = std::move(result); });
    ASSERT_TRUE(waitUntil([&] { return done.has_value(); }));

    ASSERT_FALSE(done->has_value());
    EXPECT_EQ(done->error().kind, ApiErrorKind::Unauthenticated);
    EXPECT_TRUE(server_.requests().empty());
}

// --- Cancellation tied to the caller ----------------------------------------------------------------

TEST_F(ClientAuthTest, DestroyingTheContextCancelsTheCallSilently) {
    server_.enqueue([] {
        FakeResponse response = FakeResponse::proto({});
        response.delay = 300ms;
        return response;
    }());
    auto owner = std::make_unique<QObject>();
    int completions = 0;

    auto handle = client_->unary(
        contract::kAuthGetCurrentUser, authv1::GetCurrentUserRequest{}, owner.get(),
        [&completions](ApiResult<authv1::GetCurrentUserResponse>) { ++completions; });
    ASSERT_TRUE(waitUntil([&] { return !server_.requests().empty(); }));
    EXPECT_TRUE(handle.isActive());

    owner.reset(); // the caller goes away while the request is in flight
    EXPECT_FALSE(handle.isActive());
    waitUntil([] { return false; }, 400ms);

    EXPECT_EQ(completions, 0) << "the completion of a destroyed caller must never run";
}

TEST_F(ClientAuthTest, ContextBoundCallsStillCompleteNormallyWhileTheContextLives) {
    server_.enqueue(FakeResponse::proto(userResponse()));
    QObject owner;
    std::optional<ApiResult<authv1::GetCurrentUserResponse>> out;

    client_->unary(contract::kAuthGetCurrentUser, authv1::GetCurrentUserRequest{}, &owner,
                   [&out](ApiResult<authv1::GetCurrentUserResponse> result) { out = std::move(result); });
    ASSERT_TRUE(waitUntil([&] { return out.has_value(); }));

    EXPECT_TRUE(out->has_value());
}

TEST_F(ClientAuthTest, CallScopeCancelsEverythingItHoldsWhenDestroyed) {
    FakeResponse slow = FakeResponse::proto(userResponse());
    slow.delay = 300ms;
    server_.enqueue(slow);
    server_.enqueue(slow);
    int completions = 0;
    {
        CallScope scope;
        for (int i = 0; i < 2; ++i) {
            scope.add(client_->unary(contract::kAuthGetCurrentUser, authv1::GetCurrentUserRequest{},
                                     [&completions](ApiResult<authv1::GetCurrentUserResponse>) { ++completions; }));
        }
        EXPECT_EQ(scope.activeCount(), 2U);
        ASSERT_TRUE(waitUntil([&] { return server_.requests().size() == 2U; }));
    }
    waitUntil([] { return false; }, 400ms);

    EXPECT_EQ(completions, 0);
}

// --- Exceptions never reach the event loop -------------------------------------------------------------

TEST_F(ClientAuthTest, AThrowingCompletionIsContainedAndLaterCallsStillWork) {
    server_.enqueue(FakeResponse::proto(userResponse()));
    server_.enqueue(FakeResponse::proto(userResponse()));
    bool threw = false;
    client_->unary(contract::kAuthGetCurrentUser, authv1::GetCurrentUserRequest{},
                   [&threw](ApiResult<authv1::GetCurrentUserResponse>) {
                       threw = true;
                       throw std::runtime_error("boom");
                   });
    ASSERT_TRUE(waitUntil([&] { return threw; }));

    std::optional<ApiResult<authv1::GetCurrentUserResponse>> out;
    call(contract::kAuthGetCurrentUser, authv1::GetCurrentUserRequest{}, out);
    ASSERT_TRUE(waitUntil([&] { return out.has_value(); }));
    EXPECT_TRUE(out->has_value());
}

TEST_F(ClientAuthTest, AThrowingStreamHandlerEndsTheStreamWithoutCrashing) {
    server_.enqueue(FakeResponse::connectStream(
        {envelope::encode(0, loginResponse("m1")) + envelope::encode(envelope::kFlagEndStream, "{}")}));
    std::optional<ApiResult<void>> done;

    client_->serverStream(
        kStream, authv1::LoginRequest{}, [](authv1::LoginResponse) { throw std::runtime_error("handler failed"); },
        [&done](ApiResult<void> result) { done = std::move(result); });
    ASSERT_TRUE(waitUntil([&] { return done.has_value(); }));
    SUCCEED(); // reaching this line means the exception did not terminate the process
}

} // namespace
} // namespace aedst::network
