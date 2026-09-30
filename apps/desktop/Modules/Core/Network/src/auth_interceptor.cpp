#include "aedst/network/auth_interceptor.hpp"

#include <QByteArray>
#include <QNetworkRequest>

#include <utility>

namespace aedst::network {

AuthInterceptor::AuthInterceptor(std::shared_ptr<const TokenProvider> tokenProvider)
    : tokenProvider_(std::move(tokenProvider)) {}

void AuthInterceptor::apply(QNetworkRequest& request, AuthRequirement requirement) const {
    if (requirement == AuthRequirement::Public || !tokenProvider_) {
        return;
    }
    const auto token = tokenProvider_->accessToken();
    if (!token || token->empty()) {
        return;
    }
    request.setRawHeader("Authorization", QByteArray("Bearer ") + QByteArray::fromStdString(*token));
}

} // namespace aedst::network
