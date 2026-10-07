#include "aedst/updater/qt_http_fetcher.hpp"

#include <QByteArray>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

#include <memory>
#include <string>

namespace aedst::updater {

core::Outcome<std::string> QtHttpFetcher::fetch(std::string_view url, std::size_t maxBytes) {
    const QUrl target(QString::fromUtf8(url.data(), static_cast<qsizetype>(url.size())), QUrl::StrictMode);
    if (!target.isValid() || target.scheme() != QLatin1String("https") || target.host().isEmpty() ||
        !target.userInfo().isEmpty()) {
        return core::fail(core::Error::invalidArgument("update: only plain https URLs can be fetched"));
    }

    QNetworkAccessManager network; // lives on this (worker) thread
    QNetworkRequest request(target);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setRawHeader("Accept", "application/json");
    request.setHeader(QNetworkRequest::UserAgentHeader, QByteArrayLiteral("AEDST-Desktop-Updater"));

    QNetworkReply* reply = network.get(request);
    QEventLoop loop;
    bool tooLarge = false;
    bool timedOut = false;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::downloadProgress, &loop, [&](qint64 received, qint64) {
        if (received > static_cast<qint64>(maxBytes)) {
            tooLarge = true;
            reply->abort();
        }
    });
    QTimer deadline;
    deadline.setSingleShot(true);
    QObject::connect(&deadline, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        reply->abort();
    });
    deadline.start(static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(timeout_).count()));
    loop.exec();

    const std::unique_ptr<QNetworkReply, void (*)(QNetworkReply*)> owned(reply,
                                                                          [](QNetworkReply* r) { r->deleteLater(); });
    if (tooLarge) {
        return core::fail(core::Error::invalidArgument("update: response larger than allowed"));
    }
    if (timedOut) {
        return core::fail(core::Error::network("update: the server did not answer in time"));
    }
    if (reply->error() != QNetworkReply::NoError) {
        return core::fail(core::Error::network("update: " + reply->errorString().toStdString()));
    }
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status != 200) {
        return core::fail(core::Error::network("update: HTTP " + std::to_string(status)));
    }
    const QByteArray body = reply->readAll();
    if (static_cast<std::size_t>(body.size()) > maxBytes) {
        return core::fail(core::Error::invalidArgument("update: response larger than allowed"));
    }
    return std::string(body.constData(), static_cast<std::size_t>(body.size()));
}

} // namespace aedst::updater
