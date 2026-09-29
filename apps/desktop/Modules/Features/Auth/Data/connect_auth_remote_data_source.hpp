#pragma once

#include "Data/auth_remote_data_source.hpp"

#include <memory>

namespace aedst::network {
class ApiClient;
} // namespace aedst::network

namespace aedst::feature::auth {

/// AuthRemoteDataSource over AedstNetwork's Connect-JSON ApiClient. The only
/// file in this feature that knows procedure paths and wire DTO decoding.
class ConnectAuthRemoteDataSource final : public AuthRemoteDataSource {
public:
    explicit ConnectAuthRemoteDataSource(std::shared_ptr<network::ApiClient> client);

    void login(const network::dto::LoginRequestDto& request,
               network::ApiCompletion<network::dto::LoginResponseDto> done) override;
    void refreshToken(const network::dto::RefreshTokenRequestDto& request,
                      network::ApiCompletion<network::dto::RefreshTokenResponseDto> done) override;
    void logout(const network::dto::LogoutRequestDto& request, network::ApiCompletion<void> done) override;

private:
    std::shared_ptr<network::ApiClient> client_;
};

} // namespace aedst::feature::auth
