#include "aedst/media/presigned_uploader.hpp"

#include "aedst/logging/log_categories.hpp"
#include "aedst/network/transport_policy.hpp"

#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSsl>
#include <QSslConfiguration>
#include <QSslSocket>

namespace aedst::media {

namespace {

/// Host + path only: presigned query strings are credentials and must never be logged.
[[nodiscard]] std::string redactedUrl(const QUrl& url) {
    return url.adjusted(QUrl::RemoveQuery | QUrl::RemoveUserInfo | QUrl::RemoveFragment).toString().toStdString();
}

} // namespace

PresignedUploader::PresignedUploader(QNetworkAccessManager& network, bool allowInsecureLoopback, QObject* parent)
    : QObject(parent), network_(network), allowInsecureLoopback_(allowInsecureLoopback) {}

PresignedUploader::~PresignedUploader() {
    if (reply_) {
        reply_->disconnect(this);
        reply_->abort();
    }
}

bool PresignedUploader::start(const QUrl& url, const QByteArray& method, const QHash<QByteArray, QByteArray>& headers,
                              const QString& filePath, const QByteArray& contentType, QString* error) {
    auto fail = [error](const QString& message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };
    if (reply_) {
        return fail(QStringLiteral("An upload is already running"));
    }
    // Same rule as the API endpoint, minus the query check: presigned URLs carry their
    // signature in the query string by design.
    const QUrl withoutQuery = url.adjusted(QUrl::RemoveQuery);
    if (auto valid = network::validateEndpoint(withoutQuery, allowInsecureLoopback_); !valid) {
        return fail(QString::fromStdString(valid.error().message));
    }
    const QByteArray verb = method.isEmpty() ? QByteArrayLiteral("PUT") : method.toUpper();
    if (verb != "PUT" && verb != "POST") {
        return fail(QStringLiteral("Unsupported upload method"));
    }

    file_ = std::make_unique<QFile>(filePath);
    if (!file_->open(QIODevice::ReadOnly)) {
        const QString message = file_->errorString();
        file_.reset();
        return fail(message);
    }

    QNetworkRequest request(url);
    QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
    ssl.setProtocol(QSsl::TlsV1_2OrLater);
    ssl.setPeerVerifyMode(QSslSocket::VerifyPeer);
    request.setSslConfiguration(ssl);
    request.setHeader(QNetworkRequest::ContentLengthHeader, file_->size());
    bool hasContentType = false;
    for (auto it = headers.cbegin(); it != headers.cend(); ++it) {
        if (it.key().compare("authorization", Qt::CaseInsensitive) == 0) {
            continue; // never forward credentials-looking headers from a server response
        }
        hasContentType = hasContentType || it.key().compare("content-type", Qt::CaseInsensitive) == 0;
        request.setRawHeader(it.key(), it.value());
    }
    if (!hasContentType && !contentType.isEmpty()) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    }
    // Object stores do not speak HTTP/2 consistently for large PUTs; let Qt decide but
    // never follow redirects with the body (a redirect would re-send the file elsewhere).
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);

    cancelled_ = false;
    reply_ = network_.sendCustomRequest(request, verb, file_.get());
    connect(reply_, &QNetworkReply::uploadProgress, this, &PresignedUploader::progress);
    connect(reply_, &QNetworkReply::finished, this, &PresignedUploader::onFinished);
    qCInfo(lcMedia, "Upload started (%lld bytes to %s)", static_cast<long long>(file_->size()),
           redactedUrl(url).c_str());
    return true;
}

void PresignedUploader::cancel() {
    if (reply_) {
        cancelled_ = true;
        reply_->abort(); // finished() follows synchronously with OperationCanceledError
    }
}

void PresignedUploader::onFinished() {
    QNetworkReply* reply = reply_.data();
    if (reply == nullptr) {
        return;
    }
    reply_.clear();
    reply->deleteLater();
    file_.reset();

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (cancelled_ || reply->error() == QNetworkReply::OperationCanceledError) {
        emit finished(Outcome::Cancelled, QString());
        return;
    }
    if (reply->error() == QNetworkReply::NoError && status >= 200 && status < 300) {
        emit finished(Outcome::Succeeded, QString());
        return;
    }
    qCWarning(lcMedia, "Upload failed (HTTP %d, %s)", status, reply->errorString().toStdString().c_str());
    if (status == 403) {
        emit finished(Outcome::Expired, reply->errorString());
    } else if (status == 0 || status == 408 || status == 429 || status >= 500) {
        emit finished(Outcome::Retryable, reply->errorString());
    } else {
        emit finished(Outcome::Failed, reply->errorString());
    }
}

} // namespace aedst::media
