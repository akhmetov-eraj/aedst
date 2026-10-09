#pragma once

#include "aedst/core/outcome.hpp"
#include "aedst/oidc/oidc_config.hpp"

#include <chrono>
#include <string>
#include <string_view>

namespace aedst::oidc {

/// Who signed in, as far as the UI needs to know. Not a credential.
struct OidcUser {
    std::string subject;
    std::string email;
    std::string name;

    friend bool operator==(const OidcUser&, const OidcUser&) = default;
};

/// Validates an ID token that was received directly from the token endpoint over TLS
/// (OpenID Connect Core 3.1.3.7: the TLS server check replaces the signature check) and returns
/// the user. Checks, in order: JWS compact shape, `iss` equals the configured issuer, `aud`
/// contains the client id (and `azp` equals it when `aud` has several entries), `exp` is in the
/// future (allowing `leeway`), `iat` is not far in the future, `sub` is present. When `expectedNonce` is not empty
/// (an interactive sign-in sent one in the authorization request) the `nonce` claim must be present and equal to it
/// (Core 3.1.3.7 step 11); the refresh path passes none, since refreshed ID tokens need not carry a nonce.
///
/// The signature is NOT verified here and the result must never be used for authorization:
/// the API re-checks the access token on every call.
[[nodiscard]] core::Outcome<OidcUser> validateIdToken(std::string_view jwt, const OidcConfig& config,
                                                      std::chrono::system_clock::time_point now,
                                                      std::chrono::seconds leeway = std::chrono::seconds{60},
                                                      std::string_view expectedNonce = {});

/// Base64url (RFC 7515) decoding of one JWS segment; nullopt-equivalent empty string on bad input.
[[nodiscard]] std::string decodeBase64Url(std::string_view segment);

} // namespace aedst::oidc
