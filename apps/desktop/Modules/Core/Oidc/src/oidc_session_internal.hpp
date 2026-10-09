#pragma once

// Private to the Oidc module: what the OidcSession translation units share
// (oidc_session.cpp, oidc_session_signin.cpp, oidc_session_signout.cpp).

#include "aedst/core/guarded.hpp"
#include "aedst/oidc/oidc_session.hpp"

#include <QOAuth2AuthorizationCodeFlow>
#include <QPointer>
#include <QString>
#include <QTimer>

#include <chrono>

namespace aedst::oidc {

struct OidcSession::SignInAttempt {
    core::Completion<OidcUser> done;
    QPointer<QOAuth2AuthorizationCodeFlow> flow;
    QPointer<QTimer> timeout;
    bool exchanging{false};
    /// Random per attempt, sent as the authorization request's `nonce` and required back in the ID token.
    QString nonce;
};

namespace internal {

[[nodiscard]] inline std::chrono::milliseconds toMs(std::chrono::seconds seconds) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(seconds);
}

/// Maps an OAuth error response to a core::Error (defined in oidc_session.cpp).
[[nodiscard]] core::Error serverError(const QString& error, const QString& description);

} // namespace internal

} // namespace aedst::oidc
