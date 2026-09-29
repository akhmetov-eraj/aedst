#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>

class QNetworkRequest;

namespace aedst::network {

/// Where the interceptor reads the current access token from. Implemented by
/// Features/Auth's LocalTokenStoreAdapter; Network itself never knows about
/// sessions or storage.
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

/// Adds `Authorization: Bearer <token>` to every non-public procedure.
///
/// Scope cut: no automatic refresh-and-retry on 401 yet. The app refreshes once
/// at startup (AppDependencies::refreshSessionIfNeeded) and routes to login on
/// Unauthenticated; a retrying interceptor is the follow-up once sessions are
/// long-lived enough for mid-session expiry to matter.
class AuthInterceptor final {
public:
    explicit AuthInterceptor(std::shared_ptr<const TokenProvider> tokenProvider);

    void apply(QNetworkRequest& request, std::string_view procedurePath) const;

    /// Procedures callable without a session (see the server's public edge).
    [[nodiscard]] static bool isPublicProcedure(std::string_view procedurePath) noexcept;

private:
    std::shared_ptr<const TokenProvider> tokenProvider_;
};

} // namespace aedst::network
