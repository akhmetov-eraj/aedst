#pragma once

#include "aedst/network/api_error.hpp"
#include "aedst/network/dto/login_dto.hpp"

namespace aedst::feature::auth {

/// The public aedst.auth.v1.AuthService surface (Login / RefreshToken / Logout),
/// matching apps/server/internal/modules/auth/api.Service's client-reachable
/// methods.
///
/// CreateCredential and IssueSession also exist on the server's api.Service,
/// but they are in-process only (called by users.RegisterUser) and not exposed
/// over ConnectRPC — so they have no client counterpart here. A desktop sign-up
/// flow would call aedst.users.v1.UsersService/RegisterUser, which belongs to
/// Features/Users.
class AuthRemoteDataSource {
public:
    AuthRemoteDataSource() = default;
    AuthRemoteDataSource(const AuthRemoteDataSource&) = delete;
    AuthRemoteDataSource& operator=(const AuthRemoteDataSource&) = delete;
    AuthRemoteDataSource(AuthRemoteDataSource&&) = delete;
    AuthRemoteDataSource& operator=(AuthRemoteDataSource&&) = delete;
    virtual ~AuthRemoteDataSource() = default;

    virtual void login(const network::dto::LoginRequestDto& request,
                       network::ApiCompletion<network::dto::LoginResponseDto> done) = 0;
    virtual void refreshToken(const network::dto::RefreshTokenRequestDto& request,
                              network::ApiCompletion<network::dto::RefreshTokenResponseDto> done) = 0;
    virtual void logout(const network::dto::LogoutRequestDto& request, network::ApiCompletion<void> done) = 0;
};

} // namespace aedst::feature::auth
