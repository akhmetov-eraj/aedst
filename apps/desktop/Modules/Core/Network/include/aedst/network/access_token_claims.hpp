#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aedst::network {

/// The few access-token claims the client uses for UI decisions only.
struct AccessTokenClaims {
    std::string subject;
    std::vector<std::string> roles;

    [[nodiscard]] bool hasRole(std::string_view role) const;

    friend bool operator==(const AccessTokenClaims&, const AccessTokenClaims&) = default;
};

/// Decodes the payload of a JWS compact access token (`header.payload.signature`)
/// WITHOUT verifying the signature. This is deliberate and safe only because the
/// result decides what the UI shows (e.g. the Admin entry): the server re-checks
/// every call (httpserver.RoleInterceptor + AEDST_ROLE_POLICY), so a forged token
/// gains nothing but a screen whose RPCs fail with permission_denied.
///
/// `rolesClaim` mirrors the server's AEDST_OIDC_ROLES_CLAIM: a top-level name
/// ("roles", the first-party issuer's claim) or a dotted path
/// ("realm_access.roles" for Keycloak). A claim name that itself contains dots
/// wins over path splitting, like the server. Accepted shapes: an array of
/// strings, a single space-separated string, or an object whose keys are roles.
/// Returns nullopt for anything that is not a well-formed JWT payload.
[[nodiscard]] std::optional<AccessTokenClaims> parseAccessTokenClaims(std::string_view token,
                                                                      std::string_view rolesClaim = "roles");

} // namespace aedst::network
