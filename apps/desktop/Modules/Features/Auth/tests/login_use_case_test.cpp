#include "Domain/auth_repository.hpp"
#include "Domain/login_use_case.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <utility>

namespace aedst::feature::auth {
namespace {

using ::testing::_;
using ::testing::Field;
using ::testing::StrictMock;

class MockAuthRepository : public AuthRepository {
public:
    MOCK_METHOD(void, login, (const Credentials& credentials, core::Completion<Session> done), (override));
    MOCK_METHOD(void, refreshToken, (core::Completion<Session> done), (override));
    MOCK_METHOD(void, logout, (core::Completion<void> done), (override));
    MOCK_METHOD(std::optional<Session>, currentSession, (), (const, override));
};

class LoginUseCaseTest : public ::testing::Test {
protected:
    std::shared_ptr<StrictMock<MockAuthRepository>> repository_ = std::make_shared<StrictMock<MockAuthRepository>>();
    LoginUseCase useCase_{repository_};
    std::optional<core::Outcome<Session>> received_;

    core::Completion<Session> capture() {
        return [this](core::Outcome<Session> result) { received_ = std::move(result); };
    }
};

TEST_F(LoginUseCaseTest, RejectsMalformedEmailWithoutCallingRepository) {
    useCase_.execute({.email = "not-an-email", .password = "secret"}, capture());

    ASSERT_TRUE(received_.has_value());
    ASSERT_FALSE(received_->has_value());
    EXPECT_EQ(received_->error().code, core::ErrorCode::InvalidInput);
}

TEST_F(LoginUseCaseTest, RejectsEmptyPasswordWithoutCallingRepository) {
    useCase_.execute({.email = "user@aedst.dev", .password = ""}, capture());

    ASSERT_TRUE(received_.has_value());
    ASSERT_FALSE(received_->has_value());
    EXPECT_EQ(received_->error().message, "Password is required");
}

TEST_F(LoginUseCaseTest, ForwardsValidCredentialsAndPropagatesSession) {
    const Session expected{.userId = "user-1", .accessToken = "a", .refreshToken = "r", .accessTokenExpiresAt = {}};
    EXPECT_CALL(*repository_, login(Field(&Credentials::email, "user@aedst.dev"), _))
        .WillOnce([&expected](const Credentials&, core::Completion<Session> done) { done(expected); });

    useCase_.execute({.email = "user@aedst.dev", .password = "secret"}, capture());

    ASSERT_TRUE(received_.has_value());
    ASSERT_TRUE(received_->has_value());
    EXPECT_EQ(**received_, expected);
}

TEST_F(LoginUseCaseTest, PropagatesRepositoryFailure) {
    EXPECT_CALL(*repository_, login(_, _)).WillOnce([](const Credentials&, core::Completion<Session> done) {
        done(core::fail(core::Error::unauthenticated("invalid credentials")));
    });

    useCase_.execute({.email = "user@aedst.dev", .password = "wrong"}, capture());

    ASSERT_TRUE(received_.has_value());
    ASSERT_FALSE(received_->has_value());
    EXPECT_EQ(received_->error().code, core::ErrorCode::Unauthenticated);
}

TEST(LoginUseCaseValidationTest, AcceptsWellFormedCredentials) {
    EXPECT_FALSE(LoginUseCase::validate({.email = "a@b.co", .password = "x"}).has_value());
    EXPECT_TRUE(LoginUseCase::validate({.email = "@b.co", .password = "x"}).has_value());
    EXPECT_TRUE(LoginUseCase::validate({.email = "a@", .password = "x"}).has_value());
    EXPECT_TRUE(LoginUseCase::validate({.email = "a@@b", .password = "x"}).has_value());
}

} // namespace
} // namespace aedst::feature::auth
