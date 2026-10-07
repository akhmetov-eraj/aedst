#include "aedst/telemetry/error_center.hpp"
#include "aedst/telemetry/posthog_analytics_tracker.hpp"
#include "aedst/testing/test_main.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTimeZone>
#include <gtest/gtest.h>

namespace aedst::telemetry {
namespace {

using core::Error;
using core::ErrorVisibility;

TEST(PostHogPayloadTest, BatchCarriesKeyAndEvents) {
    const QList<PostHogEvent> events{PostHogEvent{.event = QStringLiteral("task_completed"),
                                                  .distinctId = QStringLiteral("user-1"),
                                                  .properties = QJsonObject{{QStringLiteral("k"), QStringLiteral("v")}},
                                                  .timestamp = QDateTime::fromSecsSinceEpoch(0, QTimeZone::UTC)}};
    const QJsonObject body = QJsonDocument::fromJson(buildBatchPayload(QStringLiteral("phc_test"), events)).object();
    EXPECT_EQ(body.value(QStringLiteral("api_key")).toString(), QStringLiteral("phc_test"));
    const QJsonArray batch = body.value(QStringLiteral("batch")).toArray();
    ASSERT_EQ(batch.size(), 1);
    const QJsonObject first = batch.at(0).toObject();
    EXPECT_EQ(first.value(QStringLiteral("event")).toString(), QStringLiteral("task_completed"));
    EXPECT_EQ(first.value(QStringLiteral("distinct_id")).toString(), QStringLiteral("user-1"));
    EXPECT_EQ(first.value(QStringLiteral("timestamp")).toString(), QStringLiteral("1970-01-01T00:00:00.000Z"));
}

TEST(PostHogPayloadTest, ParsesV2AndLegacyFlagShapes) {
    const auto v2 = parseFeatureFlags(
        R"({"flags":{"new-ui":{"enabled":true,"variant":null},"exp":{"enabled":true,"variant":"b"},"off":{"enabled":false}}})");
    EXPECT_EQ(v2.value(QStringLiteral("new-ui")), QVariant(true));
    EXPECT_EQ(v2.value(QStringLiteral("exp")), QVariant(QStringLiteral("b")));
    EXPECT_EQ(v2.value(QStringLiteral("off")), QVariant(false));

    const auto legacy = parseFeatureFlags(R"({"featureFlags":{"a":true,"b":"control"}})");
    EXPECT_EQ(legacy.value(QStringLiteral("a")), QVariant(true));
    EXPECT_EQ(legacy.value(QStringLiteral("b")), QVariant(QStringLiteral("control")));
    EXPECT_TRUE(parseFeatureFlags("not json").isEmpty());
}

TEST(PostHogTrackerTest, DisabledWithoutKeyOrConsent) {
    PostHogAnalyticsTracker unconfigured(PostHogConfig{});
    unconfigured.setConsent(true);
    unconfigured.track("x", {});
    EXPECT_FALSE(unconfigured.isEnabled());
    EXPECT_EQ(unconfigured.queuedEventCount(), 0);

    PostHogAnalyticsTracker noConsent(PostHogConfig{.apiKey = QStringLiteral("phc_test")});
    EXPECT_TRUE(noConsent.isConfigured());
    noConsent.track("x", {});
    EXPECT_EQ(noConsent.queuedEventCount(), 0);
}

TEST(PostHogTrackerTest, RejectsPlainHttpHost) {
    PostHogAnalyticsTracker tracker(
        PostHogConfig{.apiKey = QStringLiteral("phc_test"), .host = QUrl(QStringLiteral("http://posthog.example"))});
    EXPECT_FALSE(tracker.isConfigured());
}

TEST(PostHogTrackerTest, RevokingConsentDropsQueuedEvents) {
    // Loopback https host: the flag reload that consent triggers fails fast, nothing leaves the machine.
    PostHogAnalyticsTracker tracker(PostHogConfig{.apiKey = QStringLiteral("phc_test"),
                                                  .host = QUrl(QStringLiteral("https://127.0.0.1:9")),
                                                  .maxBatchSize = 100});
    tracker.setConsent(true);
    tracker.track("a", {});
    tracker.track("b", {});
    EXPECT_EQ(tracker.queuedEventCount(), 2);
    tracker.setConsent(false);
    EXPECT_EQ(tracker.queuedEventCount(), 0);
}

TEST(ErrorCenterTest, MapsCodesToStableKinds) {
    EXPECT_EQ(ErrorCenter::kindFor(core::ErrorCode::Network), QStringLiteral("network"));
    EXPECT_EQ(ErrorCenter::kindFor(core::ErrorCode::PermissionDenied), QStringLiteral("permission_denied"));
    EXPECT_TRUE(ErrorCenter::isRetryable(core::ErrorCode::Network));
    EXPECT_FALSE(ErrorCenter::isRetryable(core::ErrorCode::InvalidArgument));
}

TEST(ErrorCenterTest, EmitsOnceForDuplicatesAndNeverForSilentOrCancelled) {
    ErrorCenter center;
    QSignalSpy raised(&center, &ErrorCenter::errorRaised);
    QSignalSpy expired(&center, &ErrorCenter::sessionExpired);

    center.report(Error::network("offline"), "tasks.list");
    center.report(Error::network("offline"), "tasks.list");
    center.report(Error::cancelled("user"), "tasks.list");
    center.report(Error::notFound("gone"), "tasks.detail", ErrorVisibility::Silent);
    EXPECT_EQ(raised.count(), 1);

    center.report(Error::unauthenticated("expired"), "tasks.list", ErrorVisibility::Blocking);
    EXPECT_EQ(expired.count(), 1);
    ASSERT_EQ(raised.count(), 2);
    EXPECT_EQ(raised.at(1).at(0).toString(), QStringLiteral("unauthenticated"));
    EXPECT_TRUE(raised.at(1).at(3).toBool());
}

} // namespace
} // namespace aedst::telemetry

AEDST_GTEST_MAIN_WITH_QCOREAPPLICATION()
