#pragma once

#include "aedst/network/api_error.hpp"

#include <aedst/auth/v1/auth.pb.h>

namespace aedst::feature::auth {

namespace authv1 = ::aedst::auth::v1;

/// The client-reachable subset of aedst.auth.v1.AuthService this feature uses
/// (Login / RefreshToken / Logout), speaking the generated Protobuf messages.
/// Register and GetCurrentUser exist on the server and in aedst/contract/methods.hpp
/// but no desktop flow calls them yet.
class AuthRemoteDataSource {
public:
    AuthRemoteDataSource() = default;
    AuthRemoteDataSource(const AuthRemoteDataSource&) = delete;
    AuthRemoteDataSource& operator=(const AuthRemoteDataSource&) = delete;
    AuthRemoteDataSource(AuthRemoteDataSource&&) = delete;
    AuthRemoteDataSource& operator=(AuthRemoteDataSource&&) = delete;
    virtual ~AuthRemoteDataSource() = default;

    virtual void login(const authv1::LoginRequest& request, network::ApiCompletion<authv1::LoginResponse> done) = 0;
    virtual void refreshToken(const authv1::RefreshTokenRequest& request,
                              network::ApiCompletion<authv1::RefreshTokenResponse> done) = 0;
    virtual void logout(const authv1::LogoutRequest& request, network::ApiCompletion<void> done) = 0;
};

} // namespace aedst::feature::auth
