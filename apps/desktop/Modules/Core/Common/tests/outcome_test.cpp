#include "aedst/core/outcome.hpp"
#include "aedst/core/use_case.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <string>

namespace aedst::core {
namespace {

Outcome<int> parsePositive(int value) {
    if (value <= 0) {
        return fail(Error::invalidInput("must be positive"));
    }
    return value;
}

class DoubleUseCase final : public UseCase<int, int> {
public:
    void execute(int input, Completion<int> done) override {
        auto parsed = parsePositive(input);
        if (!parsed) {
            done(fail(parsed.error()));
            return;
        }
        done(*parsed * 2);
    }
};

TEST(OutcomeTest, HoldsValueOnSuccess) {
    const auto outcome = parsePositive(3);

    ASSERT_TRUE(outcome.has_value());
    EXPECT_EQ(*outcome, 3);
}

TEST(OutcomeTest, HoldsTypedErrorOnFailure) {
    const auto outcome = parsePositive(-1);

    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().code, ErrorCode::InvalidInput);
    EXPECT_EQ(outcome.error().message, "must be positive");
}

TEST(OutcomeTest, VoidOutcomeOkHasValue) {
    EXPECT_TRUE(ok().has_value());
}

TEST(OutcomeTest, ErrorsCompareByCodeAndMessage) {
    EXPECT_EQ(Error::unavailable("offline"), Error::unavailable("offline"));
    EXPECT_NE(Error::unavailable("offline"), Error::storage("offline"));
}

TEST(OutcomeTest, ErrorCodeHasStableWireName) {
    EXPECT_EQ(toString(ErrorCode::Unauthenticated), "unauthenticated");
    EXPECT_EQ(toString(ErrorCode::Unknown), "unknown");
    EXPECT_EQ(toString(ErrorCode::UpgradeRequired), "upgradeRequired");
    EXPECT_EQ(toString(ErrorCode::InvalidInput), "invalidInput");
    EXPECT_EQ(toString(ErrorCode::Forbidden), "forbidden");
    EXPECT_EQ(toString(ErrorCode::RateLimited), "rateLimited");
}

TEST(UseCaseTest, CompletionReceivesMappedResult) {
    DoubleUseCase useCase;
    std::optional<Outcome<int>> received;

    useCase.execute(21, [&](Outcome<int> result) { received = std::move(result); });

    ASSERT_TRUE(received.has_value());
    ASSERT_TRUE(received->has_value());
    EXPECT_EQ(**received, 42);
}

TEST(UseCaseTest, CompletionReceivesError) {
    DoubleUseCase useCase;
    std::optional<Outcome<int>> received;

    useCase.execute(0, [&](Outcome<int> result) { received = std::move(result); });

    ASSERT_TRUE(received.has_value());
    ASSERT_FALSE(received->has_value());
    EXPECT_EQ(received->error().code, ErrorCode::InvalidInput);
}

} // namespace
} // namespace aedst::core
