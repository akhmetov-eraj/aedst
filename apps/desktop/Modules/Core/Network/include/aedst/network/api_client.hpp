#pragma once

#include "aedst/network/api_error.hpp"

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>

#include <chrono>
#include <memory>
#include <string_view>

class QNetworkReply;

namespace aedst::network {

class AuthInterceptor;

using JsonResult = ApiResult<QJsonObject>;
using JsonCompletion = ApiCompletion<QJsonObject>;

/// Unary ConnectRPC client over Qt Network, using Connect's JSON codec
/// (`POST <base>/<package.Service>/<Method>`, `Connect-Protocol-Version: 1`).
///
/// Speaking the protocol directly — instead of generated stubs as web/apple
/// do — means the desktop build needs no protoc/buf step; the price is that
/// DTOs in dto/ are hand-mirrored from apps/server/api/proto and must be kept
/// in sync on proto changes. Completions run on the thread that owns this
/// object (the GUI thread in the app).
class ApiClient final : public QObject {
    Q_OBJECT

public:
    ApiClient(QUrl baseUrl, std::shared_ptr<const AuthInterceptor> interceptor, QObject* parent = nullptr);

    void call(std::string_view procedurePath, const QJsonObject& body, JsonCompletion done);

    void setTimeout(std::chrono::milliseconds timeout) noexcept;
    [[nodiscard]] const QUrl& baseUrl() const noexcept;

private:
    [[nodiscard]] static JsonResult parseReply(QNetworkReply& reply);

    QNetworkAccessManager network_;
    QUrl baseUrl_;
    std::shared_ptr<const AuthInterceptor> interceptor_;
    std::chrono::milliseconds timeout_{std::chrono::seconds{15}};
};

} // namespace aedst::network
