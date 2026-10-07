#pragma once

#include "aedst/core/outcome.hpp"
#include "aedst/network/api_error.hpp"
#include "aedst/network/method.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>

class QNetworkRequest;

namespace aedst::network {

/// Where the interceptor reads the current access token from. Implemented by
/// Features/Auth's LocalTokenStoreAdapter (first-party login) and by oidc::OidcSession;
/// Network itself never knows about sessions or storage.
class TokenProvider {
public:
    TokenProvider() = default;
    TokenProvider(const TokenProvider&) = delete;
    TokenProvider& operator=(const TokenProvider&) = delete;
    TokenProvider(TokenProvider&&) = delete;
    TokenProvider& operator=(TokenProvider&&) = delete;
    virtual ~TokenProvider() = default;

    [[nodiscard]] virtual std::optional<std::string> accessToken() const = 0;
};

/// Optional capability of a token source: obtain a new access token. oidc::OidcSession implements it
/// with a single-flight refresh, so any number of concurrent calls trigger exactly one token request.
/// Completions may run synchronously (token still fresh) or later on the event loop.
class TokenRefresher {
public:
    TokenRefresher() = default;
    TokenRefresher(const TokenRefresher&) = delete;
    TokenRefresher& operator=(const TokenRefresher&) = delete;
    TokenRefresher(TokenRefresher&&) = delete;
    TokenRefresher& operator=(TokenRefresher&&) = delete;
    virtual ~TokenRefresher() = default;

    /// A token that stays valid for a while; refreshes first when the current one is about to expire.
    /// ErrorCode::Unauthenticated = the session is over; ErrorCode::Network = could not refresh now.
    virtual void ensureFreshAccessToken(core::Completion<std::string> done) = 0;

    /// The API answered `unauthenticated` for `rejectedToken`. If a newer token already exists it is
    /// returned without another request.
    virtual void refreshAfterRejection(const std::string& rejectedToken, core::Completion<std::string> done) = 0;
};

/// Adds `Authorization: Bearer <token>` to every request whose method is
/// AuthRequirement::Required. ConnectClient calls it once per attempt, so a
/// retry after a token rotation picks up the new token.
///
/// With a TokenRefresher the interceptor also keeps the session alive for the client:
///  - prepare(): before every attempt of an authenticated call the token is refreshed if it is
///    about to expire (no request is sent with a token known to be stale);
///  - refreshAfterRejection(): when the server still answers `unauthenticated`, the token is
///    refreshed ONCE and the call is replayed once; a second rejection is final.
/// Transient refresh failures never end the session: prepare() lets the request go out with the
/// current token, refreshAfterRejection() reports a transport error instead of Unauthenticated.
class AuthInterceptor final {
public:
    /// nullopt = go ahead; an ApiError = complete the call with it, send nothing.
    using Verdict = std::function<void(std::optional<ApiError>)>;

    explicit AuthInterceptor(std::shared_ptr<const TokenProvider> tokenProvider,
                             std::shared_ptr<TokenRefresher> refresher = nullptr);

    void apply(QNetworkRequest& request, AuthRequirement requirement) const;

    [[nodiscard]] bool canRefresh() const noexcept { return refresher_ != nullptr; }

    void prepare(AuthRequirement requirement, Verdict ready) const;
    void refreshAfterRejection(const std::string& rejectedToken, Verdict retry) const;

private:
    std::shared_ptr<const TokenProvider> tokenProvider_;
    std::shared_ptr<TokenRefresher> refresher_;
};

} // namespace aedst::network
