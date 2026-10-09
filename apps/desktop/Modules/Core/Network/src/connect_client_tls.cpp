// TLS policy for ConnectClient: configuration, SPKI pinning. Split out of connect_client.cpp
// without behaviour change.

#include "connect_client_internal.hpp"

#include <QCryptographicHash>
#include <QSslCertificate>
#include <QSslKey>
#include <QSslSocket>

namespace aedst::network::internal {

ApiError pinError() {
    return {.kind = ApiErrorKind::Transport,
            .httpStatus = 0,
            .message = "TLS certificate pinning failed: the server's public key matches none of the pinned hashes"};
}

namespace {

constexpr int kSha256Bytes = 32;

[[nodiscard]] QByteArray spkiSha256(const QSslCertificate& certificate) {
    return QCryptographicHash::hash(certificate.publicKey().toDer(), QCryptographicHash::Sha256);
}

} // namespace

/// Everything TLS-related the client enforces, or the reason the config is unusable.
ApiResult<TlsSetup> buildTlsSetup(const ClientConfig& config) {
    if (auto endpoint = validateEndpoint(config.baseUrl, config.allowInsecureLoopback); !endpoint) {
        return std::unexpected(std::move(endpoint.error()));
    }

    TlsSetup setup;
    setup.ssl = QSslConfiguration::defaultConfiguration();
    // TlsV1_2OrLater: TLS 1.3 wherever the backend supports it, never below 1.2.
    setup.ssl.setProtocol(QSsl::TlsV1_2OrLater);
    setup.ssl.setPeerVerifyMode(QSslSocket::VerifyPeer);

    if (config.baseUrl.scheme() == QLatin1String("https") && !QSslSocket::supportsSsl()) {
        return invalidConfig("TLS is not available in this Qt build: cannot reach an https endpoint");
    }

    if (!config.extraCaCertificatesPem.isEmpty()) {
        const QList<QSslCertificate> extra = QSslCertificate::fromData(config.extraCaCertificatesPem, QSsl::Pem);
        if (extra.isEmpty()) {
            return invalidConfig("extraCaCertificatesPem contains no valid PEM certificate");
        }
        QList<QSslCertificate> trusted = setup.ssl.caCertificates();
        if (trusted.isEmpty()) {
            trusted = QSslConfiguration::systemCaCertificates();
        }
        trusted.append(extra);
        setup.ssl.setCaCertificates(trusted);
    }

    for (const std::string& pin : config.pinnedSpkiSha256) {
        const QByteArray digest = QByteArray::fromBase64(
            QByteArray::fromStdString(pin), QByteArray::Base64Encoding | QByteArray::AbortOnBase64DecodingErrors);
        if (digest.size() != kSha256Bytes) {
            return invalidConfig("pinnedSpkiSha256 entries must be base64 of a 32-byte SHA-256 digest");
        }
        setup.pins.append(digest);
    }
    return setup;
}

/// True (and the reply is marked, and aborted if still running) when pinning is
/// on for this https reply and the peer chain contains none of the pinned keys.
/// Fails closed: a successful reply that exposes no peer chain counts as a violation.
/// Safe to call repeatedly (encrypted, readyRead, finished).
bool pinViolation(QNetworkReply& reply, const QList<QByteArray>& pins) {
    if (pins.isEmpty() || reply.url().scheme() != QLatin1String("https")) {
        return false;
    }
    if (reply.property(kPinFailedProperty).toBool()) {
        return true;
    }
    const QList<QSslCertificate> chain = reply.sslConfiguration().peerCertificateChain();
    bool violated = true;
    if (chain.isEmpty()) {
        // No handshake happened (refused, DNS failure...): that is the transport
        // error's business. Only a *successful* reply without a chain is suspect.
        violated = reply.error() == QNetworkReply::NoError;
    } else {
        for (const QSslCertificate& certificate : chain) {
            if (!certificate.publicKey().isNull() && pins.contains(spkiSha256(certificate))) {
                violated = false;
                break;
            }
        }
    }
    if (violated) {
        reply.setProperty(kPinFailedProperty, true);
        if (reply.isRunning()) {
            reply.abort();
        }
    }
    return violated;
}

/// Pins are checked as soon as the handshake completes (before the response is
/// read), and again on readyRead/finished as defence in depth.
void watchHandshake(QNetworkReply* reply, const QList<QByteArray>& pins) {
    if (pins.isEmpty()) {
        return;
    }
    QObject::connect(reply, &QNetworkReply::encrypted, reply, [reply, pins]() { pinViolation(*reply, pins); });
}

} // namespace aedst::network::internal
