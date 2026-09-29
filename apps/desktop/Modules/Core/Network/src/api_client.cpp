#include "aedst/network/api_client.hpp"

#include "aedst/core/logger.hpp"
#include "aedst/network/auth_interceptor.hpp"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <string>
#include <utility>

namespace aedst::network {

using core::AedstLogger;

ApiClient::ApiClient(QUrl baseUrl, std::shared_ptr<const AuthInterceptor> interceptor, QObject* parent)
    : QObject(parent), baseUrl_(std::move(baseUrl)), interceptor_(std::move(interceptor)) {
    // Replies are deleteLater()'d after finished(), so the slot below may still read them.
    network_.setAutoDeleteReplies(true);
}

void ApiClient::call(std::string_view procedurePath, const QJsonObject& body, JsonCompletion done) {
    QUrl url = baseUrl_;
    url.setPath(baseUrl_.path() +
                QString::fromUtf8(procedurePath.data(), static_cast<qsizetype>(procedurePath.size())));

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Connect-Protocol-Version", "1");
    request.setTransferTimeout(timeout_);
    if (interceptor_) {
        interceptor_->apply(request, procedurePath);
    }

    std::string path(procedurePath);
    AedstLogger::debug("POST {}", path);

    QNetworkReply* reply = network_.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [reply, path = std::move(path), done = std::move(done)]() {
        JsonResult result = parseReply(*reply);
        if (!result) {
            AedstLogger::warn("{} failed: http={} message={}", path, result.error().httpStatus,
                              result.error().message);
        }
        done(std::move(result));
    });
}

void ApiClient::setTimeout(std::chrono::milliseconds timeout) noexcept {
    timeout_ = timeout;
}

const QUrl& ApiClient::baseUrl() const noexcept {
    return baseUrl_;
}

JsonResult ApiClient::parseReply(QNetworkReply& reply) {
    const int status = reply.attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray payload = reply.readAll();

    // setTransferTimeout() aborts the reply, which Qt reports as OperationCanceledError.
    if (reply.error() == QNetworkReply::OperationCanceledError || reply.error() == QNetworkReply::TimeoutError) {
        return std::unexpected(ApiError{ApiErrorKind::Timeout, status, reply.errorString().toStdString()});
    }
    if (status == 0) {
        return std::unexpected(ApiError{ApiErrorKind::Transport, 0, reply.errorString().toStdString()});
    }

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);

    if (status >= 200 && status < 300) {
        if (payload.isEmpty()) {
            return QJsonObject{};
        }
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            return std::unexpected(
                ApiError{ApiErrorKind::Decoding, status, parseError.errorString().toStdString()});
        }
        return document.object();
    }

    // Connect error body: {"code": "unauthenticated", "message": "..."}
    const QJsonObject errorBody = document.isObject() ? document.object() : QJsonObject{};
    const std::string code = errorBody.value(QStringLiteral("code")).toString().toStdString();
    std::string message = errorBody.value(QStringLiteral("message")).toString(reply.errorString()).toStdString();
    return std::unexpected(ApiError::fromConnectResponse(status, code, std::move(message)));
}

} // namespace aedst::network
