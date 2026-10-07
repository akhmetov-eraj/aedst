#include "aedst/oidc/oidc_session.hpp"

#include "aedst/logging/log_categories.hpp"

#include <QDateTime>
#include <QDesktopServices>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QOAuth2AuthorizationCodeFlow>
#include <QOAuthHttpServerReplyHandler>
#include <QPointer>
#include <QTimer>
#include <QUrlQuery>

#include <ctime>
#include <string>
#include <memory>
#include <utility>

namespace aedst::oidc {
namespace {

constexpr auto kFallbackLifetime = std::chrono::minutes{5};

[[nodiscard]] std::chrono::milliseconds toMs(std::chrono::seconds seconds) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(seconds);
}

/// OAuth error codes that mean "this refresh token will never work again".
[[nodiscard]] bool isTerminalGrantError(const QString& error) {
    return error == QLatin1String("invalid_grant") || error == QLatin1String("invalid_token") ||
           error == QLatin1String("invalid_client") || error == QLatin1String("unauthorized_client");
}

[[nodiscard]] core::Error serverError(const QString& error, const QString& description) {
    std::string message = "OIDC provider reported: " + error.toStdString();
    if (!description.isEmpty()) {
        message += " (" + description.toStdString() + ")";
    }
    if (error == QLatin1String("access_denied")) {
        return core::Error::cancelled("Sign-in was cancelled or denied");
    }
    if (isTerminalGrantError(error)) {
        return core::Error::unauthenticated(std::move(message));
    }
    return core::Error::network(std::move(message)); // temporarily_unavailable, server_error, ...
}

[[nodiscard]] QByteArray formField(const QByteArray& name, const QString& value) {
    return name + '=' + QUrl::toPercentEncoding(value);
}

} // namespace

struct OidcSession::SignInAttempt {
    core::Completion<OidcUser> done;
    QPointer<QOAuth2AuthorizationCodeFlow> flow;
    QPointer<QTimer> timeout;
    bool exchanging{false};
};

OidcSession::OidcSession(OidcConfig config, Dependencies dependencies, QObject* parent)
    : QObject(parent), config_(std::move(config)), deps_(std::move(dependencies)) {
    if (!deps_.clock) {
        deps_.clock = std::make_shared<core::SystemClock>();
    }
    if (!deps_.openBrowser) {
        deps_.openBrowser = [](const QUrl& url) { return QDesktopServices::openUrl(url); };
    }
}

OidcSession::~OidcSession() = default;

std::chrono::system_clock::time_point OidcSession::now() const {
    return deps_.clock->now();
}

// --- State ----------------------------------------------------------------------------------

std::optional<std::string> OidcSession::accessToken() const {
    if (accessToken_.empty()) {
        return std::nullopt;
    }
    return accessToken_.reveal();
}

bool OidcSession::isSignedIn() const noexcept {
    return !refreshToken_.empty();
}

std::optional<OidcUser> OidcSession::user() const {
    return user_;
}

std::chrono::system_clock::time_point OidcSession::accessTokenExpiresAt() const noexcept {
    return expiresAt_;
}

bool OidcSession::hasValidAccessToken() const {
    return !accessToken_.empty() && now() + config_.refreshSkew < expiresAt_;
}

bool OidcSession::signInInProgress() const noexcept {
    return signIn_ != nullptr;
}

void OidcSession::clearMemory() {
    const bool wasSignedIn = isSignedIn();
    accessToken_.clear();
    refreshToken_.clear();
    idToken_.clear();
    expiresAt_ = {};
    user_.reset();
    ++epoch_;
    refreshing_ = false;
    if (wasSignedIn) {
        emit signedInChanged();
    }
}

void OidcSession::applyTokens(Tokens tokens) {
    const bool wasSignedIn = isSignedIn();
    accessToken_ = std::move(tokens.access);
    expiresAt_ = tokens.expiresAt;
    if (!tokens.idToken.empty()) {
        idToken_ = std::move(tokens.idToken);
        if (auto claims = validateIdToken(idToken_.reveal(), config_, now())) {
            user_ = std::move(*claims);
        }
    }
    const bool rotated = !tokens.refresh.empty() && tokens.refresh.reveal() != refreshToken_.reveal();
    if (rotated) {
        refreshToken_ = std::move(tokens.refresh);
        if (wasSignedIn) {
            persistRotatedRefreshToken(); // sign-in persists through its own, awaited path
        }
    }
    if (!wasSignedIn && isSignedIn()) {
        emit signedInChanged();
    }
}

void OidcSession::persistRotatedRefreshToken() {
    if (!user_ || refreshToken_.empty()) {
        return;
    }
    deps_.store->save(StoredIdentity{.refreshToken = refreshToken_, .subject = user_->subject},
                      [](core::VoidOutcome saved) {
                          if (!saved) {
                              // The in-memory session keeps working; the next start will need a fresh sign-in.
                              qCWarning(lcAuth, "Could not persist the rotated refresh token: %s",
                                        saved.error().message.c_str());
                          }
                      });
}

// --- Discovery ------------------------------------------------------------------------------

void OidcSession::resolveEndpoints(core::Completion<OidcEndpoints> done) {
    if (endpoints_) {
        done(*endpoints_);
        return;
    }
    endpointWaiters_.push_back(std::move(done));
    if (endpointWaiters_.size() > 1) {
        return; // a discovery request is already in flight
    }
    QPointer<OidcSession> self(this);
    fetchDiscovery(*deps_.network, config_, [self](core::Outcome<OidcEndpoints> result) {
        if (!self) {
            return;
        }
        if (result) {
            self->endpoints_ = *result;
        }
        auto waiters = std::exchange(self->endpointWaiters_, {});
        for (auto& waiter : waiters) {
            waiter(result);
        }
    });
}

// --- Sign-in --------------------------------------------------------------------------------

void OidcSession::signIn(core::Completion<OidcUser> done) {
    if (auto valid = validateOidcConfig(config_); !valid) {
        done(core::fail(valid.error()));
        return;
    }
    if (signIn_) {
        done(core::fail(core::Error::invalidArgument("A sign-in is already in progress")));
        return;
    }
    signIn_ = std::make_unique<SignInAttempt>();
    signIn_->done = std::move(done);

    QPointer<OidcSession> self(this);
    resolveEndpoints([self](core::Outcome<OidcEndpoints> endpoints) {
        if (!self || !self->signIn_) {
            return; // destroyed or cancelled while discovering
        }
        if (!endpoints) {
            self->finishSignIn(core::fail(endpoints.error()));
            return;
        }
        self->startAuthorization(*endpoints);
    });
}

void OidcSession::startAuthorization(const OidcEndpoints& endpoints) {
    auto* flow = new QOAuth2AuthorizationCodeFlow(deps_.network, this);
    flow->setClientIdentifier(config_.clientId);
    flow->setAuthorizationUrl(endpoints.authorization);
    flow->setTokenUrl(endpoints.token);
    flow->setRequestedScopeTokens(config_.scopeTokens());
    flow->setPkceMethod(QOAuth2AuthorizationCodeFlow::PkceMethod::S256);

    // RFC 8252: loopback redirect on 127.0.0.1 with a port the OS picks (or the configured fixed one).
    auto* handler = new QOAuthHttpServerReplyHandler(QHostAddress::LocalHost, config_.loopbackPort, flow);
    if (!handler->isListening()) {
        flow->deleteLater();
        finishSignIn(core::fail(core::Error::network("Could not open a loopback port for the sign-in redirect")));
        return;
    }
    handler->setCallbackPath(QStringLiteral("/callback"));
    handler->setCallbackText(QStringLiteral(
        "<!doctype html><html><head><meta charset=\"utf-8\"><title>AEDST</title></head><body style=\"font-family:"
        "sans-serif;text-align:center;margin-top:20vh\"><h2>AEDST</h2><p>You can close this tab and return to the "
        "application.</p></body></html>"));
    flow->setReplyHandler(handler);

    signIn_->flow = flow;
    signIn_->timeout = new QTimer(flow);
    signIn_->timeout->setSingleShot(true);

    QPointer<OidcSession> self(this);
    QObject::connect(signIn_->timeout, &QTimer::timeout, flow, [self]() {
        if (self) {
            self->finishSignIn(core::fail(core::Error::cancelled("Sign-in timed out")));
        }
    });
    QObject::connect(flow, &QAbstractOAuth::authorizeWithBrowser, flow, [self](const QUrl& url) {
        if (self && !self->deps_.openBrowser(url)) {
            self->finishSignIn(core::fail(core::Error::unknown("Could not open the system browser")));
        }
    });
    QObject::connect(flow, &QAbstractOAuth2::serverReportedErrorOccurred, flow,
                     [self](const QString& error, const QString& description, const QUrl&) {
                         if (self) {
                             self->finishSignIn(core::fail(serverError(error, description)));
                         }
                     });
    QObject::connect(flow, &QAbstractOAuth::requestFailed, flow, [self](QAbstractOAuth::Error error) {
        if (self) {
            self->finishSignIn(core::fail(core::Error::network("Sign-in request failed (Qt OAuth error " +
                                                               std::to_string(static_cast<int>(error)) + ")")));
        }
    });
    QObject::connect(flow, &QAbstractOAuth::granted, flow, [self, flow]() {
        if (!self || !self->signIn_ || self->signIn_->exchanging) {
            return;
        }
        self->signIn_->exchanging = true;
        self->completeSignIn(self->readTokens(*flow));
    });

    signIn_->timeout->start(static_cast<int>(toMs(config_.signInTimeout).count()));
    qCInfo(lcAuth, "Starting browser sign-in (loopback port %d)", static_cast<int>(handler->port()));
    flow->grant();
}

OidcSession::Tokens OidcSession::readTokens(const QOAuth2AuthorizationCodeFlow& flow) const {
    Tokens tokens;
    tokens.access = core::SecretString(flow.token().toStdString());
    tokens.refresh = core::SecretString(flow.refreshToken().toStdString());
    tokens.idToken = core::SecretString(flow.idToken().toStdString());
    const QDateTime expiration = flow.expirationAt();
    tokens.expiresAt = expiration.isValid()
                           ? std::chrono::system_clock::from_time_t(static_cast<std::time_t>(expiration.toSecsSinceEpoch()))
                           : now() + kFallbackLifetime;
    return tokens;
}

void OidcSession::completeSignIn(Tokens tokens) {
    if (tokens.access.empty()) {
        finishSignIn(core::fail(core::Error::unauthenticated("The provider did not return an access token")));
        return;
    }
    if (tokens.refresh.empty()) {
        finishSignIn(core::fail(core::Error::unauthenticated(
            "The provider did not return a refresh token; check that the application allows refresh tokens "
            "(scope offline_access)")));
        return;
    }
    auto user = validateIdToken(tokens.idToken.reveal(), config_, now());
    if (!user) {
        finishSignIn(core::fail(user.error()));
        return;
    }

    QPointer<OidcSession> self(this);
    const StoredIdentity identity{.refreshToken = tokens.refresh, .subject = user->subject};
    deps_.store->save(identity, [self, tokens = std::move(tokens), user = *user](core::VoidOutcome saved) mutable {
        if (!self) {
            return;
        }
        if (!saved) {
            // Never keep a session that cannot be restored securely: discard the grant at the provider.
            qCWarning(lcAuth, "Sign-in aborted, credential storage unavailable: %s", saved.error().message.c_str());
            if (self->endpoints_) {
                self->revokeRefreshToken(*self->endpoints_, tokens.refresh, [] {});
            }
            self->finishSignIn(core::fail(saved.error()));
            return;
        }
        self->refreshToken_ = tokens.refresh; // applyTokens() sees "unchanged" and does not re-save
        self->applyTokens(std::move(tokens));
        self->user_ = user;
        emit self->signedInChanged();
        self->finishSignIn(user);
    });
}

void OidcSession::finishSignIn(core::Outcome<OidcUser> result) {
    if (!signIn_) {
        return;
    }
    const auto attempt = std::move(signIn_);
    if (attempt->timeout) {
        attempt->timeout->stop();
    }
    if (attempt->flow) {
        attempt->flow->deleteLater(); // also closes the loopback server (child of the flow)
    }
    if (result) {
        qCInfo(lcAuth, "Browser sign-in completed");
    } else {
        qCInfo(lcAuth, "Browser sign-in did not complete: %s", result.error().message.c_str());
    }
    if (attempt->done) {
        attempt->done(std::move(result));
    }
}

void OidcSession::cancelSignIn() {
    finishSignIn(core::fail(core::Error::cancelled("Sign-in was cancelled")));
}

// --- Restore and refresh ---------------------------------------------------------------------

void OidcSession::restore(core::Completion<std::optional<OidcUser>> done) {
    if (isSignedIn() && hasValidAccessToken()) {
        done(user_);
        return;
    }
    QPointer<OidcSession> self(this);
    deps_.store->load([self, done = std::move(done)](core::Outcome<std::optional<StoredIdentity>> loaded) {
        if (!self) {
            return;
        }
        if (!loaded) {
            done(core::fail(loaded.error()));
            return;
        }
        if (!loaded->has_value()) {
            done(std::optional<OidcUser>{});
            return;
        }
        self->refreshToken_ = std::move((*loaded)->refreshToken);
        self->user_ = OidcUser{.subject = (*loaded)->subject, .email = {}, .name = {}};
        emit self->signedInChanged();
        self->refreshWaiters_.push_back([self, done](core::Outcome<std::string> refreshed) {
            if (!self) {
                return;
            }
            if (!refreshed) {
                done(core::fail(refreshed.error())); // offline keeps the identity; rejected already cleared it
                return;
            }
            done(self->user_);
        });
        self->startRefresh();
    });
}

void OidcSession::ensureFreshAccessToken(core::Completion<std::string> done) {
    if (!isSignedIn()) {
        done(core::fail(core::Error::unauthenticated("Not signed in")));
        return;
    }
    if (hasValidAccessToken()) {
        done(accessToken_.reveal());
        return;
    }
    refreshWaiters_.push_back(std::move(done));
    startRefresh();
}

void OidcSession::refreshAfterRejection(const std::string& rejectedToken, core::Completion<std::string> done) {
    if (!isSignedIn()) {
        done(core::fail(core::Error::unauthenticated("Not signed in")));
        return;
    }
    // Several in-flight calls fail with the same stale token: only the first one refreshes.
    if (!refreshing_ && hasValidAccessToken() && accessToken_.reveal() != rejectedToken) {
        done(accessToken_.reveal());
        return;
    }
    refreshWaiters_.push_back(std::move(done));
    startRefresh();
}

void OidcSession::startRefresh() {
    if (refreshing_) {
        return; // single flight: the waiter queued by the caller piggybacks on the running request
    }
    refreshing_ = true;
    const std::uint64_t epoch = epoch_;
    QPointer<OidcSession> self(this);
    resolveEndpoints([self, epoch](core::Outcome<OidcEndpoints> endpoints) {
        if (!self || epoch != self->epoch_) {
            return;
        }
        if (!endpoints) {
            self->finishRefresh(epoch, core::fail(endpoints.error()));
            return;
        }
        self->runRefresh(*endpoints);
    });
}

void OidcSession::runRefresh(const OidcEndpoints& endpoints) {
    const std::uint64_t epoch = epoch_;
    auto* flow = new QOAuth2AuthorizationCodeFlow(deps_.network, this);
    flow->setClientIdentifier(config_.clientId);
    flow->setAuthorizationUrl(endpoints.authorization);
    flow->setTokenUrl(endpoints.token);
    flow->setRequestedScopeTokens(config_.scopeTokens());
    flow->setRefreshToken(QString::fromStdString(refreshToken_.reveal()));

    auto* guard = new QTimer(flow);
    guard->setSingleShot(true);

    // Qt reports the outcome through signals whose combination differs between server and network
    // failures; whichever comes first settles the refresh, later ones are ignored.
    auto settled = std::make_shared<bool>(false);
    QPointer<OidcSession> self(this);
    auto settle = [self, flow, settled, epoch](core::Outcome<Tokens> result) {
        if (*settled) {
            return;
        }
        *settled = true;
        flow->deleteLater();
        if (self) {
            self->finishRefresh(epoch, std::move(result));
        }
    };

    QObject::connect(flow, &QAbstractOAuth::statusChanged, flow, [flow, settle, self](QAbstractOAuth::Status status) {
        if (status == QAbstractOAuth::Status::Granted && self) {
            settle(self->readTokens(*flow));
        }
    });
    QObject::connect(flow, &QAbstractOAuth2::serverReportedErrorOccurred, flow,
                     [settle](const QString& error, const QString& description, const QUrl&) {
                         settle(core::fail(serverError(error, description)));
                     });
    QObject::connect(flow, &QAbstractOAuth::requestFailed, flow, [settle](QAbstractOAuth::Error error) {
        // No OAuth error body: treat as transport trouble and keep the session.
        settle(core::fail(core::Error::network("Token refresh request failed (Qt OAuth error " +
                                               std::to_string(static_cast<int>(error)) + ")")));
    });
    QObject::connect(guard, &QTimer::timeout, flow, [settle]() {
        settle(core::fail(core::Error::network("Token refresh timed out")));
    });
    guard->start(static_cast<int>(toMs(config_.requestTimeout * 2).count()));
    flow->refreshTokens();
}

void OidcSession::finishRefresh(std::uint64_t epoch, core::Outcome<Tokens> result) {
    if (epoch != epoch_) {
        return; // signOut() happened meanwhile; its waiters were already failed
    }
    refreshing_ = false;
    if (result) {
        if (result->access.empty()) {
            result = core::fail(core::Error::unauthenticated("The provider returned an empty access token"));
        }
    }
    if (!result) {
        const core::Error error = result.error();
        if (error.code == core::ErrorCode::Unauthenticated) {
            qCWarning(lcAuth, "Refresh token rejected; the session has ended: %s", error.message.c_str());
            clearMemory();
            deps_.store->clear([](core::VoidOutcome cleared) {
                if (!cleared) {
                    qCWarning(lcAuth, "Could not clear the stored identity: %s", cleared.error().message.c_str());
                }
            });
            failRefreshWaiters(error);
            emit sessionExpired();
            return;
        }
        qCWarning(lcAuth, "Token refresh failed, keeping the session: %s", error.message.c_str());
        failRefreshWaiters(error);
        return;
    }
    applyTokens(std::move(*result));
    qCInfo(lcAuth, "Access token refreshed");
    auto waiters = std::exchange(refreshWaiters_, {});
    for (auto& waiter : waiters) {
        waiter(accessToken_.reveal());
    }
}

void OidcSession::failRefreshWaiters(const core::Error& error) {
    auto waiters = std::exchange(refreshWaiters_, {});
    for (auto& waiter : waiters) {
        waiter(core::fail(error));
    }
}

// --- Sign-out --------------------------------------------------------------------------------

void OidcSession::signOut(core::Completion<void> done) {
    cancelSignIn();
    const core::SecretString refresh = refreshToken_;
    const core::SecretString idToken = idToken_;
    clearMemory();
    failRefreshWaiters(core::Error::unauthenticated("Signed out"));

    QPointer<OidcSession> self(this);
    deps_.store->clear([self, refresh, idToken, done = std::move(done)](core::VoidOutcome stored) {
        // The user is signed out locally from here on; tell the caller now and finish the
        // provider-side cleanup in the background (it may take a network timeout when offline).
        done(stored);
        if (!self || refresh.empty()) {
            return;
        }
        self->resolveEndpoints([self, refresh, idToken](core::Outcome<OidcEndpoints> endpoints) {
            if (!self || !endpoints) {
                return;
            }
            self->revokeRefreshToken(*endpoints, refresh, [self, endpoints = *endpoints, idToken]() {
                if (self) {
                    self->openEndSession(endpoints, idToken);
                }
            });
        });
    });
}

void OidcSession::revokeRefreshToken(const OidcEndpoints& endpoints, const core::SecretString& refreshToken,
                                     std::function<void()> finished) {
    if (endpoints.revocation.isEmpty() || refreshToken.empty()) {
        finished();
        return;
    }
    QNetworkRequest request(endpoints.revocation);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/x-www-form-urlencoded"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(static_cast<int>(toMs(config_.requestTimeout).count()));

    QByteArray body = formField("token", QString::fromStdString(refreshToken.reveal()));
    body += "&token_type_hint=refresh_token&" + formField("client_id", config_.clientId);

    QNetworkReply* reply = deps_.network->post(request, body);
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, finished = std::move(finished)]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcAuth, "Token revocation failed: %s", qUtf8Printable(reply->errorString()));
        } else {
            qCInfo(lcAuth, "Refresh token revoked");
        }
        finished();
    });
}

void OidcSession::openEndSession(const OidcEndpoints& endpoints, const core::SecretString& idToken) {
    if (!config_.endSessionInBrowser || endpoints.endSession.isEmpty()) {
        return;
    }
    QUrl url = endpoints.endSession;
    QUrlQuery query(url);
    if (!idToken.empty()) {
        query.addQueryItem(QStringLiteral("id_token_hint"), QString::fromStdString(idToken.reveal()));
    }
    query.addQueryItem(QStringLiteral("client_id"), config_.clientId);
    url.setQuery(query);
    if (!deps_.openBrowser(url)) {
        qCWarning(lcAuth, "Could not open the browser to end the provider session");
    }
}

} // namespace aedst::oidc
