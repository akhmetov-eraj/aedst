#include "aedst/capabilities/capability_rules.hpp"
#include "capability_test_support.hpp"

#include <gtest/gtest.h>

namespace aedst::capabilities {
namespace {

using namespace testsupport;

TEST(CapabilityRulesTest, DefaultsWithoutAnAnswerAreBuiltInsOnAndFlaggedOff) {
    const auto set = evaluateCapabilities(fixtureBuild(), nullptr);
    EXPECT_TRUE(set.isKnown());
    EXPECT_TRUE(set.hasCapability("tasks"));
    EXPECT_TRUE(set.hasCapability("auth"));
    EXPECT_FALSE(set.hasCapability("admin"));
    EXPECT_FALSE(set.isFlagEnabled("admin"));
}

TEST(CapabilityRulesTest, ServerCanSwitchABuiltInOffAndAFlaggedFeatureOn) {
    ServerCapabilities answer;
    addFeature(answer, "search", false);
    addFeature(answer, "admin", true);
    const auto set = evaluateCapabilities(fixtureBuild(), &answer);
    EXPECT_FALSE(set.hasCapability("search"));
    EXPECT_TRUE(set.hasCapability("admin"));
    EXPECT_TRUE(set.isFlagEnabled("admin"));
    EXPECT_TRUE(set.hasCapability("tasks")) << "a feature the server does not list keeps its default";
}

TEST(CapabilityRulesTest, UnknownServerFeaturesAreIgnored) {
    ServerCapabilities answer;
    addFeature(answer, "teleport", true);
    addFeature(answer, "teleport.beam", true);
    const auto set = evaluateCapabilities(fixtureBuild(), &answer);
    EXPECT_FALSE(set.hasCapability("teleport"));
    EXPECT_FALSE(set.hasCapability("teleport.beam"));
}

TEST(CapabilityRulesTest, RequiredClientVersionHidesOnOlderClientsAndShowsOnEqualOrNewer) {
    ServerCapabilities answer;
    addFeature(answer, "media", true, "1.4.0");
    EXPECT_FALSE(evaluateCapabilities(fixtureBuild("1.3.0"), &answer).hasCapability("media"));
    EXPECT_TRUE(evaluateCapabilities(fixtureBuild("1.4.0"), &answer).hasCapability("media"));
    EXPECT_TRUE(evaluateCapabilities(fixtureBuild("2.0.0"), &answer).hasCapability("media"));
    EXPECT_FALSE(evaluateCapabilities(fixtureBuild("1.4.0-rc.1"), &answer).hasCapability("media"))
        << "a pre-release sorts before its release";
}

TEST(CapabilityRulesTest, UnparsableVersionsFailClosedOnlyWhereARequirementExists) {
    ServerCapabilities answer;
    addFeature(answer, "media", true, "not-a-version");
    addFeature(answer, "tasks", true);
    EXPECT_FALSE(evaluateCapabilities(fixtureBuild("1.3.0"), &answer).hasCapability("media"));
    EXPECT_TRUE(evaluateCapabilities(fixtureBuild(""), &answer).hasCapability("tasks"));

    ServerCapabilities requirement;
    addFeature(requirement, "media", true, "1.0.0");
    EXPECT_FALSE(evaluateCapabilities(fixtureBuild(""), &requirement).hasCapability("media"));
}

TEST(CapabilityRulesTest, SubCapabilitiesNeedTheirCoarseParentToBeShown) {
    ServerCapabilities answer;
    addFeature(answer, "media.uploads", true);
    addFeature(answer, "search.fuzzy", true);
    addFeature(answer, "search", false);
    const auto set = evaluateCapabilities(fixtureBuild(), &answer);
    EXPECT_TRUE(set.hasCapability("media.uploads"));
    EXPECT_FALSE(set.hasCapability("search.fuzzy"));
}

TEST(CapabilityRulesTest, VersionAdviceUsesTheMinimumRecommendedAndUpdateStatus) {
    ServerCapabilities answer;
    setWindow(answer, "android", "1.2.0", "1.5.0");

    EXPECT_EQ(adviseVersion(fixtureBuild("1.1.0"), &answer).advice, UpdateAdvice::Required);
    EXPECT_EQ(adviseVersion(fixtureBuild("1.3.0"), &answer).advice, UpdateAdvice::Recommended);
    EXPECT_EQ(adviseVersion(fixtureBuild("1.5.0"), &answer).advice, UpdateAdvice::None);
    EXPECT_EQ(adviseVersion(fixtureBuild("1.3.0"), &answer).minimum, "1.2.0");

    ServerCapabilities statusOnly;
    statusOnly.set_update_status(::aedst::capabilities::v1::UPDATE_STATUS_RECOMMENDED);
    EXPECT_EQ(adviseVersion(fixtureBuild("1.3.0"), &statusOnly).advice, UpdateAdvice::Recommended);

    EXPECT_EQ(adviseVersion(fixtureBuild("1.3.0"), nullptr).advice, UpdateAdvice::None);
    EXPECT_EQ(adviseVersion(fixtureBuild(""), &answer).advice, UpdateAdvice::None);

    ServerCapabilities otherPlatform;
    setWindow(otherPlatform, "web", "9.0.0", "9.0.0");
    EXPECT_EQ(adviseVersion(fixtureBuild("1.3.0"), &otherPlatform).advice, UpdateAdvice::None);
}

} // namespace
} // namespace aedst::capabilities
