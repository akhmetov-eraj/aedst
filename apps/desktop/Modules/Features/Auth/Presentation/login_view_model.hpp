#pragma once

#include "aedst/core/secure_memory.hpp"

#include <QObject>
#include <QString>

#include <memory>

namespace aedst::analytics {
class AnalyticsTracker;
} // namespace aedst::analytics

namespace aedst::navigation {
class Router;
} // namespace aedst::navigation

namespace aedst::feature::auth {

class LoginUseCase;
class LogoutUseCase;

/// Unidirectional data flow, Qt-style:
///  - State is Q_PROPERTY + NOTIFY (QML re-reads on change) — the StateFlow /
///    @Published equivalent.
///  - One-shot effects are plain signals (loginSucceeded / loginFailed /
///    loggedOut), never properties, so a re-created view can't replay a
///    navigation or a toast — the same split as Android's UiState vs Effect
///    channel.
/// Navigation after login goes through the Core Router by route name; this
/// class never references Users or Profile.
class LoginViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString email READ email WRITE setEmail NOTIFY emailChanged)
    // Write-only from QML: READ always returns an empty string so the secret is never
    // bound back into the UI; QML observes `hasPassword` instead.
    Q_PROPERTY(QString password READ password WRITE setPassword)
    Q_PROPERTY(bool hasPassword READ hasPassword NOTIFY hasPasswordChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(bool canSubmit READ canSubmit NOTIFY canSubmitChanged)

public:
    LoginViewModel(std::shared_ptr<LoginUseCase> login, std::shared_ptr<LogoutUseCase> logout,
                   std::shared_ptr<analytics::AnalyticsTracker> analytics, navigation::Router* router,
                   QObject* parent = nullptr);

    [[nodiscard]] QString email() const;
    void setEmail(const QString& email);
    /// Always empty (write-only property, see above).
    [[nodiscard]] static QString password();
    /// Stores the secret in a wiping std::string (core::SecretString), not in a QString member.
    void setPassword(const QString& password);
    [[nodiscard]] bool hasPassword() const noexcept;
    [[nodiscard]] bool isLoading() const noexcept;
    [[nodiscard]] QString errorMessage() const;
    [[nodiscard]] bool canSubmit() const;

    Q_INVOKABLE void login();
    /// Wipes the held password (also done on success, logout and destruction).
    Q_INVOKABLE void clearPassword();
    Q_INVOKABLE void logout();

signals:
    void emailChanged();
    void hasPasswordChanged();
    void isLoadingChanged();
    void errorMessageChanged();
    void canSubmitChanged();

    void loginSucceeded(const QString& userId);
    void loginFailed(const QString& message);
    void loggedOut();

private:
    void setLoading(bool loading);
    void setErrorMessage(const QString& message);

    std::shared_ptr<LoginUseCase> login_;
    std::shared_ptr<LogoutUseCase> logout_;
    std::shared_ptr<analytics::AnalyticsTracker> analytics_;
    navigation::Router* router_;

    QString email_;
    core::SecretString password_;
    QString errorMessage_;
    bool isLoading_{false};
};

} // namespace aedst::feature::auth
