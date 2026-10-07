#pragma once

#include "aedst/core/outcome.hpp"
#include "aedst/oidc/oidc_config.hpp"

#include <QByteArray>
#include <QUrl>

class QNetworkAccessManager;

namespace aedst::oidc {

/// The provider endpoints this client uses, taken from the discovery document.
struct OidcEndpoints {
    QUrl authorization;
    QUrl token;
    /// RFC 7009. Empty when the provider does not advertise one (logout then only forgets locally).
    QUrl revocation;
    /// RP-initiated logout. Empty when not advertised.
    QUrl endSession;

    friend bool operator==(const OidcEndpoints&, const OidcEndpoints&) = default;
};

/// `<issuer>/.well-known/openid-configuration`.
[[nodiscard]] QUrl discoveryUrl(const QUrl& issuer);

/// Parses and validates an OpenID Connect discovery document (pure; unit-tested):
///  - `issuer` must equal the configured issuer (mix-up attacks);
///  - authorization and token endpoints are required and must be https (or loopback http
///    when OidcConfig::allowInsecureLoopbackIssuer);
///  - when the provider lists `code_challenge_methods_supported`, it must include S256;
///  - revocation and end-session endpoints are optional but validated the same way when present.
[[nodiscard]] core::Outcome<OidcEndpoints> parseDiscoveryDocument(const QByteArray& json, const OidcConfig& config);

/// GETs the discovery document through `network` (no redirects, size-capped, timeout from the
/// config) and completes on the caller's event loop. The reply is owned by the manager and
/// deleted afterwards; `done` is invoked once per call unless the manager is destroyed first.
void fetchDiscovery(QNetworkAccessManager& network, const OidcConfig& config, core::Completion<OidcEndpoints> done);

} // namespace aedst::oidc
