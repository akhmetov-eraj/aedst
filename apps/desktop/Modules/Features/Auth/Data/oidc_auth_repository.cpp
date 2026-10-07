#include "Data/oidc_auth_repository.hpp"

#include "aedst/oidc/oidc_session.hpp"

#include <string>
#include <utility>

namespace aedst::feature::auth {

OidcAuthRepository::OidcAuthRepository(std::shared_ptr<oidc::OidcSession> session) : session_(std::move(session)) {}

void OidcAuthRepository::login(const Credentials& /*credentials*/, core::Completion<Session> done) {
    done(core::fail(core::Error::invalidArgument("Password sign-in is disabled; sign in with the browser")));
}

void OidcAuthRepository::signInWithBrowser(core::Completion<Session> done) {
    session_->signIn([this, done = std::move(done)](core::Outcome<oidc::OidcUser> user) {
        if (!user) {
            done(core::fail(user.error()));
            return;
        }
        if (auto current = currentSession()) {
            done(std::move(*current));
            return;
        }
        done(core::fail(core::Error::unauthenticated("Sign-in finished without a session")));
    });
}

void OidcAuthRepository::cancelBrowserSignIn() {
    session_->cancelSignIn();
}

void OidcAuthRepository::refreshToken(core::Completion<Session> done) {
    session_->ensureFreshAccessToken([this, done = std::move(done)](core::Outcome<std::string> token) {
        if (!token) {
            done(core::fail(token.error()));
            return;
        }
        if (auto current = currentSession()) {
            done(std::move(*current));
            return;
        }
        done(core::fail(core::Error::unauthenticated("Not signed in")));
    });
}

void OidcAuthRepository::logout(core::Completion<void> done) {
    session_->signOut(std::move(done));
}

std::optional<Session> OidcAuthRepository::currentSession() const {
    if (!session_->isSignedIn()) {
        return std::nullopt;
    }
    Session session;
    if (const auto user = session_->user()) {
        session.userId = user->subject;
    }
    if (const auto token = session_->accessToken()) {
        session.accessToken = *token;
        session.accessTokenExpiresAt = session_->accessTokenExpiresAt();
    }
    // No access token yet (offline start): the default-constructed expiry is the epoch, i.e. expired.
    return session;
}

} // namespace aedst::feature::auth
