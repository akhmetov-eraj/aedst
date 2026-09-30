#include "Presentation/login_view_model.hpp"

#include "Domain/login_use_case.hpp"
#include "Domain/logout_use_case.hpp"
#include "Navigation/auth_route.hpp"
#include "aedst/analytics/analytics_tracker.hpp"
#include "aedst/navigation/router.hpp"

#include <QPointer>

#include <string>
#include <utility>

namespace aedst::feature::auth {

LoginViewModel::LoginViewModel(std::shared_ptr<LoginUseCase> login, std::shared_ptr<LogoutUseCase> logout,
                               std::shared_ptr<analytics::AnalyticsTracker> analytics, navigation::Router* router,
                               QObject* parent)
    : QObject(parent), login_(std::move(login)), logout_(std::move(logout)), analytics_(std::move(analytics)),
      router_(router) {}

QString LoginViewModel::email() const {
    return email_;
}

void LoginViewModel::setEmail(const QString& email) {
    if (email == email_) {
        return;
    }
    email_ = email;
    emit emailChanged();
    emit canSubmitChanged();
}

QString LoginViewModel::password() {
    return {};
}

void LoginViewModel::setPassword(const QString& password) {
    // toStdString() makes the only long-lived copy, inside a SecretString that
    // wipes itself; QString/JS copies made by QML are transient and cannot be wiped.
    std::string next = password.toStdString();
    if (next == password_.reveal()) {
        core::secureWipe(next);
        return;
    }
    const bool hadPassword = !password_.empty();
    password_ = core::SecretString(std::move(next));
    if (hadPassword != !password_.empty()) {
        emit hasPasswordChanged();
    }
    emit canSubmitChanged();
}

bool LoginViewModel::hasPassword() const noexcept {
    return !password_.empty();
}

void LoginViewModel::clearPassword() {
    if (password_.empty()) {
        return;
    }
    password_.clear();
    emit hasPasswordChanged();
    emit canSubmitChanged();
}

bool LoginViewModel::isLoading() const noexcept {
    return isLoading_;
}

QString LoginViewModel::errorMessage() const {
    return errorMessage_;
}

bool LoginViewModel::canSubmit() const {
    return !isLoading_ && !email_.trimmed().isEmpty() && !password_.empty();
}

void LoginViewModel::login() {
    if (!canSubmit()) {
        return;
    }
    setErrorMessage({});
    setLoading(true);

    // Completions run later on the event loop; the guard makes a destroyed
    // ViewModel (window closed mid-request) a no-op instead of a dangling `this`.
    QPointer<LoginViewModel> guard(this);
    login_->execute(Credentials{.email = email_.trimmed().toStdString(), .password = password_},
                     [guard](core::Outcome<Session> result) {
                         if (!guard) {
                             return;
                         }
                         guard->setLoading(false);

                         if (!result) {
                             const QString message = QString::fromStdString(result.error().message);
                             guard->setErrorMessage(message);
                             if (guard->analytics_) {
                                 guard->analytics_->track(
                                     analytics::events::kLoginFailed,
                                     {{"reason", std::string(core::toString(result.error().code))}});
                             }
                             emit guard->loginFailed(message);
                             return;
                         }

                         guard->clearPassword();
                         if (guard->analytics_) {
                             guard->analytics_->identify(result->userId);
                             guard->analytics_->track(analytics::events::kLoginSucceeded, {});
                         }
                         // No post-login destination exists yet now that
                         // Users/Profile are gone (a future "tasks" module will
                         // eventually provide one), so we only signal success;
                         // the QML layer/AppDependencies own routing from here.
                         emit guard->loginSucceeded(QString::fromStdString(result->userId));
                     });
}

void LoginViewModel::logout() {
    clearPassword();
    setLoading(true);
    QPointer<LoginViewModel> guard(this);
    logout_->execute(core::NoInput{}, [guard](core::VoidOutcome result) {
        if (!guard) {
            return;
        }
        guard->setLoading(false);
        if (!result) {
            guard->setErrorMessage(QString::fromStdString(result.error().message));
        }
        if (guard->analytics_) {
            guard->analytics_->track(analytics::events::kLogout, {});
            guard->analytics_->reset();
        }
        emit guard->loggedOut();
        if (guard->router_ != nullptr) {
            guard->router_->resetTo(loginRoute());
        }
    });
}

void LoginViewModel::setLoading(bool loading) {
    if (loading == isLoading_) {
        return;
    }
    isLoading_ = loading;
    emit isLoadingChanged();
    emit canSubmitChanged();
}

void LoginViewModel::setErrorMessage(const QString& message) {
    if (message == errorMessage_) {
        return;
    }
    errorMessage_ = message;
    emit errorMessageChanged();
}

} // namespace aedst::feature::auth
