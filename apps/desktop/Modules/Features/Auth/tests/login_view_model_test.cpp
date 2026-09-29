#include "Domain/auth_repository.hpp"
#include "Domain/login_use_case.hpp"
#include "Domain/logout_use_case.hpp"
#include "Presentation/login_view_model.hpp"
#include "aedst/analytics/no_op_analytics_tracker.hpp"

#include <QSignalSpy>
#include <QTest>

#include <memory>
#include <optional>
#include <utility>

namespace aedst::feature::auth {
namespace {

/// Hand-rolled fake (not gMock) so tests can hold a login open and assert on
/// the in-flight state before completing it.
class FakeAuthRepository final : public AuthRepository {
public:
    void login(const Credentials& credentials, core::Completion<Session> done) override {
        ++loginCalls;
        lastCredentials = credentials;
        pendingLogin = std::move(done);
    }

    void refreshToken(core::Completion<Session> done) override {
        done(core::fail(core::Error::unauthenticated("not used")));
    }

    void logout(core::Completion<void> done) override {
        ++logoutCalls;
        done(core::ok());
    }

    [[nodiscard]] std::optional<Session> currentSession() const override { return std::nullopt; }

    void completeLogin(core::Outcome<Session> result) {
        auto done = std::exchange(pendingLogin, nullptr);
        done(std::move(result));
    }

    int loginCalls{0};
    int logoutCalls{0};
    Credentials lastCredentials;
    core::Completion<Session> pendingLogin;
};

} // namespace

class LoginViewModelTest : public QObject {
    Q_OBJECT

private:
    std::shared_ptr<FakeAuthRepository> repository_;
    std::unique_ptr<LoginViewModel> viewModel_;

private slots:
    void init() {
        repository_ = std::make_shared<FakeAuthRepository>();
        viewModel_ = std::make_unique<LoginViewModel>(std::make_shared<LoginUseCase>(repository_),
                                                       std::make_shared<LogoutUseCase>(repository_),
                                                       std::make_shared<analytics::NoOpAnalyticsTracker>(), nullptr);
    }

    void cleanup() {
        viewModel_.reset();
        repository_.reset();
    }

    void canSubmitRequiresEmailAndPassword() {
        QVERIFY(!viewModel_->canSubmit());

        viewModel_->setEmail(QStringLiteral("user@aedst.dev"));
        QVERIFY(!viewModel_->canSubmit());

        viewModel_->setPassword(QStringLiteral("secret"));
        QVERIFY(viewModel_->canSubmit());
    }

    void loginIsIgnoredWhenFormIsIncomplete() {
        viewModel_->setEmail(QStringLiteral("user@aedst.dev"));

        viewModel_->login();

        QCOMPARE(repository_->loginCalls, 0);
    }

    void isLoadingIsTrueWhileRequestInFlight() {
        QSignalSpy loadingSpy(viewModel_.get(), &LoginViewModel::isLoadingChanged);
        viewModel_->setEmail(QStringLiteral(" user@aedst.dev "));
        viewModel_->setPassword(QStringLiteral("secret"));

        viewModel_->login();

        QVERIFY(viewModel_->isLoading());
        QVERIFY(!viewModel_->canSubmit());
        QVERIFY(repository_->lastCredentials.email == "user@aedst.dev");

        repository_->completeLogin(
            Session{.userId = "user-1", .accessToken = "a", .refreshToken = "r", .accessTokenExpiresAt = {}});

        QVERIFY(!viewModel_->isLoading());
        QCOMPARE(loadingSpy.count(), 2);
    }

    void successfulLoginEmitsLoginSucceededAndClearsPassword() {
        QSignalSpy succeeded(viewModel_.get(), &LoginViewModel::loginSucceeded);
        QSignalSpy failed(viewModel_.get(), &LoginViewModel::loginFailed);
        viewModel_->setEmail(QStringLiteral("user@aedst.dev"));
        viewModel_->setPassword(QStringLiteral("secret"));

        viewModel_->login();
        repository_->completeLogin(
            Session{.userId = "user-1", .accessToken = "a", .refreshToken = "r", .accessTokenExpiresAt = {}});

        QCOMPARE(succeeded.count(), 1);
        QCOMPARE(succeeded.takeFirst().at(0).toString(), QStringLiteral("user-1"));
        QCOMPARE(failed.count(), 0);
        QVERIFY(viewModel_->password().isEmpty());
        QVERIFY(viewModel_->errorMessage().isEmpty());
    }

    void failedLoginExposesErrorMessageAndEmitsLoginFailed() {
        QSignalSpy failed(viewModel_.get(), &LoginViewModel::loginFailed);
        viewModel_->setEmail(QStringLiteral("user@aedst.dev"));
        viewModel_->setPassword(QStringLiteral("wrong"));

        viewModel_->login();
        repository_->completeLogin(core::fail(core::Error::unauthenticated("invalid credentials")));

        QCOMPARE(failed.count(), 1);
        QCOMPARE(viewModel_->errorMessage(), QStringLiteral("invalid credentials"));
        QCOMPARE(viewModel_->password(), QStringLiteral("wrong"));
        QVERIFY(!viewModel_->isLoading());
    }

    void logoutEmitsLoggedOut() {
        QSignalSpy loggedOut(viewModel_.get(), &LoginViewModel::loggedOut);

        viewModel_->logout();

        QCOMPARE(repository_->logoutCalls, 1);
        QCOMPARE(loggedOut.count(), 1);
    }
};

} // namespace aedst::feature::auth

QTEST_GUILESS_MAIN(aedst::feature::auth::LoginViewModelTest)
#include "login_view_model_test.moc"
