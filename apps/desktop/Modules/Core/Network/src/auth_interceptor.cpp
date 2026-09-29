#include "aedst/network/auth_interceptor.hpp"

#include "aedst/network/endpoints.hpp"

#include <QByteArray>
#include <QNetworkRequest>

#include <utility>

namespace aedst::network {

AuthInterceptor::AuthInterceptor(std::shared_ptr<const TokenProvider> tokenProvider)
    : tokenProvider_(std::move(tokenProvider)) {}

void AuthInterceptor::apply(QNetworkRequest& request, std::string_view procedurePath) const {
    if (isPublicProcedure(procedurePath) || !tokenProvider_) {
        return;
    }
    const auto token = tokenProvider_->accessToken();
    if (!token || token->empty()) {
        return;
    }
    request.setRawHeader("Authorization", QByteArray("Bearer ") + QByteArray::fromStdString(*token));
}

bool AuthInterceptor::isPublicProcedure(std::string_view procedurePath) noexcept {
    return procedurePath == endpoints::kAuthLogin || procedurePath == endpoints::kAuthRefreshToken ||
           procedurePath == endpoints::kUsersRegisterUser;
}

} // namespace aedst::network
