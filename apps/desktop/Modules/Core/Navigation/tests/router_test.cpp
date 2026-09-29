#include "aedst/navigation/router.hpp"
#include "aedst/testing/test_main.hpp"

#include <QUrl>
#include <QVariantMap>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace aedst::navigation {
namespace {

const QUrl kLoginView{QStringLiteral("qrc:/test/Login.qml")};
const QUrl kHomeView{QStringLiteral("qrc:/test/Home.qml")};
const QUrl kProfileView{QStringLiteral("qrc:/test/Profile.qml")};

// Router is generic and shouldn't depend on any real feature's routes, so
// this test registers its own made-up destinations rather than reusing
// aedst::navigation::routes:: constants (which only list routes real
// features actually navigate to).
constexpr std::string_view kHomeRoute = "test/home";
constexpr std::string_view kProfileRoute = "test/profile";

class RouterTest : public ::testing::Test {
protected:
    RouterTest() : router_(makeGraph()) {
        QObject::connect(&router_, &Router::pushRequested,
                         [this](const QUrl& view, const QVariantMap& properties) {
                             pushed_.push_back(view);
                             lastProperties_ = properties;
                         });
        QObject::connect(&router_, &Router::replaceAllRequested,
                         [this](const QUrl& view, const QVariantMap&) { replaced_.push_back(view); });
        QObject::connect(&router_, &Router::popRequested, [this]() { ++pops_; });
    }

    static std::shared_ptr<const NavGraph> makeGraph() {
        auto graph = std::make_shared<NavGraph>();
        graph->registerDestination(routes::kLogin, kLoginView);
        graph->registerDestination(kHomeRoute, kHomeView);
        graph->registerDestination(kProfileRoute, kProfileView);
        return graph;
    }

    static Route route(std::string_view name) { return Route{.name = std::string(name), .params = {}}; }

    Router router_;
    std::vector<QUrl> pushed_;
    std::vector<QUrl> replaced_;
    QVariantMap lastProperties_;
    int pops_{0};
};

TEST_F(RouterTest, PushEmitsResolvedViewAndGrowsBackStack) {
    ASSERT_TRUE(router_.resetTo(route(kHomeRoute)));
    ASSERT_TRUE(router_.push(route(kProfileRoute)));

    ASSERT_EQ(pushed_.size(), 1U);
    EXPECT_EQ(pushed_.front(), kProfileView);
    EXPECT_EQ(router_.depth(), 2);
    EXPECT_EQ(router_.currentRoute(), QStringLiteral("test/profile"));
}

TEST_F(RouterTest, UnknownRouteIsRejectedWithoutSignals) {
    EXPECT_FALSE(router_.push(route("nowhere")));

    EXPECT_TRUE(pushed_.empty());
    EXPECT_EQ(router_.depth(), 0);
}

TEST_F(RouterTest, ParamsBecomeViewProperties) {
    Route profile = route(kProfileRoute);
    profile.params.emplace(std::string(params::kUserId), "user-42");

    ASSERT_TRUE(router_.push(profile));

    EXPECT_EQ(lastProperties_.value(QStringLiteral("userId")).toString(), QStringLiteral("user-42"));
}

TEST_F(RouterTest, GoBackStopsAtRoot) {
    ASSERT_TRUE(router_.resetTo(route(kHomeRoute)));
    ASSERT_TRUE(router_.push(route(kProfileRoute)));

    EXPECT_TRUE(router_.goBack());
    EXPECT_FALSE(router_.goBack());
    EXPECT_EQ(pops_, 1);
    EXPECT_EQ(router_.depth(), 1);
}

TEST_F(RouterTest, ResetToClearsBackStack) {
    ASSERT_TRUE(router_.resetTo(route(kHomeRoute)));
    ASSERT_TRUE(router_.push(route(kProfileRoute)));

    ASSERT_TRUE(router_.replaceAll(QStringLiteral("auth/login")));

    EXPECT_EQ(router_.depth(), 1);
    ASSERT_FALSE(replaced_.empty());
    EXPECT_EQ(replaced_.back(), kLoginView);
}

} // namespace
} // namespace aedst::navigation

AEDST_GTEST_MAIN_WITH_QCOREAPPLICATION()
