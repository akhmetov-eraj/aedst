#pragma once

#include "Data/auth_remote_data_source.hpp"

#include <memory>

namespace aedst::network {
class ConnectClient;
} // namespace aedst::network

namespace aedst::feature::auth {

/// AuthRemoteDataSource over AedstNetwork's ConnectClient and the method table in
/// Aedst::Contract. The only file in this feature that knows procedure entries.
class ConnectAuthRemoteDataSource final : public AuthRemoteDataSource {
public:
    explicit ConnectAuthRemoteDataSource(std::shared_ptr<network::ConnectClient> client);

    void login(const authv1::LoginRequest& request, network::ApiCompletion<authv1::LoginResponse> done) override;
    void refreshToken(const authv1::RefreshTokenRequest& request,
                      network::ApiCompletion<authv1::RefreshTokenResponse> done) override;
    void logout(const authv1::LogoutRequest& request, network::ApiCompletion<void> done) override;

private:
    std::shared_ptr<network::ConnectClient> client_;
};

} // namespace aedst::feature::auth
