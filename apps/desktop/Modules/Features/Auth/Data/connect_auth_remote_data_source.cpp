#include "Data/connect_auth_remote_data_source.hpp"

#include "aedst/network/api_client.hpp"
#include "aedst/network/endpoints.hpp"

#include <string>
#include <utility>

namespace aedst::feature::auth {
namespace {

namespace dto = network::dto;

/// Adapts a JSON completion into a typed one, turning a malformed 2xx body into
/// a Decoding error instead of a half-filled DTO.
template <typename Dto, typename Decoder>
network::JsonCompletion decodeInto(network::ApiCompletion<Dto> done, Decoder decode, std::string what) {
    return [done = std::move(done), decode, what = std::move(what)](network::JsonResult result) {
        if (!result) {
            done(std::unexpected(std::move(result.error())));
            return;
        }
        auto decoded = decode(*result);
        if (!decoded) {
            done(std::unexpected(network::ApiError{network::ApiErrorKind::Decoding, 200, "Malformed " + what}));
            return;
        }
        done(std::move(*decoded));
    };
}

} // namespace

ConnectAuthRemoteDataSource::ConnectAuthRemoteDataSource(std::shared_ptr<network::ApiClient> client)
    : client_(std::move(client)) {}

void ConnectAuthRemoteDataSource::login(const dto::LoginRequestDto& request,
                                        network::ApiCompletion<dto::LoginResponseDto> done) {
    client_->call(network::endpoints::kAuthLogin, dto::toJson(request),
                   decodeInto(std::move(done), &dto::loginResponseFromJson, "LoginResponse"));
}

void ConnectAuthRemoteDataSource::refreshToken(const dto::RefreshTokenRequestDto& request,
                                               network::ApiCompletion<dto::RefreshTokenResponseDto> done) {
    client_->call(network::endpoints::kAuthRefreshToken, dto::toJson(request),
                   decodeInto(std::move(done), &dto::refreshTokenResponseFromJson, "RefreshTokenResponse"));
}

void ConnectAuthRemoteDataSource::logout(const dto::LogoutRequestDto& request, network::ApiCompletion<void> done) {
    client_->call(network::endpoints::kAuthLogout, dto::toJson(request),
                   [done = std::move(done)](network::JsonResult result) {
                       if (!result) {
                           done(std::unexpected(std::move(result.error())));
                           return;
                       }
                       done(network::ApiResult<void>{});
                   });
}

} // namespace aedst::feature::auth
