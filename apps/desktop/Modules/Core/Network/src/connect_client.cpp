#include "aedst/network/connect_client.hpp"

#include "aedst/core/correlation.hpp"
#include "aedst/core/logger.hpp"
#include "aedst/network/auth_interceptor.hpp"
#include "aedst/network/connect_envelope.hpp"
#include "aedst/network/connect_error.hpp"

#include <QCryptographicHash>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRandomGenerator>
#include <QSslCertificate>
#include <QSslError>
#include <QSslKey>
#include <QSslSocket>
#include <QString>
#include <QTimer>

#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace aedst::network {

using core::AedstLogger;

namespace detail {

/// Lifecycle shared between a CallHandle and the in-flight machinery.
struct CallState {
    bool cancelled{false};
    bool finished{false};
    /// The request currently on the wire, if any (empty while backing off).
    QPointer<QNetworkReply> reply;
    /// Completes the call with an error at most once. Used to cancel while no
    /// request is in flight (retry backoff); bound weakly to avoid a cycle.
    std::function<void(ApiError)> fail;
};

struct UnaryCall {
    QUrl url;
    std::string path;
    QByteArray body;
    AuthRequirement auth{AuthRequirement::Required};
    RetryPolicy retry{RetryPolicy::none()};
    std::chrono::milliseconds timeout{0};
    std::string correlationId;
    RawCompletion done;
};

struct StreamCall {
    envelope::EnvelopeReader reader;
    RawMessageHandler onMessage;
    ApiCompletion<void> onDone;
    bool contentTypeChecked{false};
};

std::optional<QByteArray> serialize(const google::protobuf::MessageLite& message) {
    const std::size_t size = message.ByteSizeLong();
    if (size > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return std::nullopt;
    }
    QByteArray bytes(static_cast<qsizetype>(size), Qt::Uninitialized);
    if (size > 0 && !message.SerializeToArray(bytes.data(), static_cast<int>(size))) {
        return std::nullopt;
    }
    return bytes;
}

} // namespace detail

namespace {

constexpr const char* kTimedOutProperty = "aedstTimedOut";
constexpr const char* kPinFailedProperty = "aedstPinFailed";
constexpr int kSha256Bytes = 32;

[[nodiscard]] ApiError cancelledError() {
    return {.kind = ApiErrorKind::Cancelled, .httpStatus = 0, .message = "Call cancelled"};
}

[[nodiscard]] ApiError pinError() {
    return {.kind = ApiErrorKind::Transport,
            .httpStatus = 0,
            .message = "TLS certificate pinning failed: the server's public key matches none of the pinned hashes"};
}

/// Server-controlled or Qt-produced text, made safe for one log line: bounded,
/// no control characters (no forged records). Secrets are masked by the logger.
[[nodiscard]] std::string loggable(std::string_view text) {
    constexpr std::size_t kMaxLoggedChars = 256;
    std::string out(text.substr(0, kMaxLoggedChars));
    for (char& c : out) {
        if (static_cast<unsigned char>(c) < 0x20 || c == 0x7F) {
            c = ' ';
        }
    }
    if (text.size() > kMaxLoggedChars) {
        out += "...";
    }
    return out;
}

/// One id per call: the caller's active CorrelationScope if it has one (so a use
/// case can tie several calls together), otherwise a fresh random id.
[[nodiscard]] std::string callCorrelationId() {
    const std::string_view active = core::currentCorrelationId();
    if (core::isValidCorrelationId(active)) {
        return std::string(active);
    }
    return core::newCorrelationId();
}

struct TlsSetup {
    QSslConfiguration ssl;
    QList<QByteArray> pins; // raw SHA-256 digests of SubjectPublicKeyInfo DER
};

[[nodiscard]] std::unexpected<ApiError> invalidConfig(std::string message) {
    return std::unexpected(ApiError{.kind = ApiErrorKind::InvalidArgument, .httpStatus = 0, .message = std::move(message)});
}

/// Everything TLS-related the client enforces, or the reason the config is unusable.
[[nodiscard]] ApiResult<TlsSetup> buildTlsSetup(const ClientConfig& config) {
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

[[nodiscard]] QByteArray spkiSha256(const QSslCertificate& certificate) {
    return QCryptographicHash::hash(certificate.publicKey().toDer(), QCryptographicHash::Sha256);
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

void hardenRequest(QNetworkRequest& request, const QSslConfiguration& ssl, const std::string& correlationId) {
    request.setSslConfiguration(ssl);
    request.setRawHeader("X-Correlation-Id", QByteArray::fromStdString(correlationId));
}

[[nodiscard]] QUrl endpointUrl(const QUrl& base, std::string_view path) {
    QUrl url = base;
    QString basePath = base.path();
    while (basePath.endsWith(QLatin1Char('/'))) {
        basePath.chop(1);
    }
    url.setPath(basePath + QString::fromUtf8(path.data(), static_cast<qsizetype>(path.size())));
    return url;
}

/// Total deadline for one attempt. QNetworkRequest::setTransferTimeout is an
/// inactivity timer, which would let a slow-drip response outlive its deadline.
void armDeadline(QNetworkReply* reply, std::chrono::milliseconds timeout) {
    if (timeout.count() <= 0) {
        return;
    }
    auto* timer = new QTimer(reply); // child of the reply: dies with it
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, reply, [reply]() {
        reply->setProperty(kTimedOutProperty, true);
        reply->abort();
    });
    timer->start(timeout);
}

[[nodiscard]] bool hasContentType(const QNetworkReply& reply, const char* expected) {
    return reply.header(QNetworkRequest::ContentTypeHeader)
        .toString()
        .startsWith(QLatin1String(expected), Qt::CaseInsensitive);
}

/// Failures with no usable HTTP response, or an aborted transfer; nullopt when
/// the reply carries a real HTTP status to interpret.
[[nodiscard]] std::optional<ApiError> transportError(const QNetworkReply& reply, int status) {
    const QNetworkReply::NetworkError netError = reply.error();
    // Before everything else: a pin violation aborts the reply, which Qt reports as
    // "operation canceled", and it must win over a body that happened to arrive.
    if (reply.property(kPinFailedProperty).toBool()) {
        return pinError();
    }
    if (reply.property(kTimedOutProperty).toBool() || netError == QNetworkReply::TimeoutError) {
        return ApiError{.kind = ApiErrorKind::Timeout, .httpStatus = status, .message = "Request timed out"};
    }
    if (netError == QNetworkReply::OperationCanceledError) {
        return cancelledError();
    }
    if (netError == QNetworkReply::SslHandshakeFailedError) {
        // sslErrors() is never answered with ignoreSslErrors(), so an untrusted,
        // expired or mismatched certificate ends up here (details are logged).
        return ApiError{.kind = ApiErrorKind::Transport,
                        .httpStatus = 0,
                        .message = "TLS certificate validation failed: " + reply.errorString().toStdString()};
    }
    if (status == 0) {
        return ApiError{.kind = ApiErrorKind::Transport, .httpStatus = 0, .message = reply.errorString().toStdString()};
    }
    return std::nullopt;
}

[[nodiscard]] RawResult parseUnaryReply(QNetworkReply& reply) {
    const int status = reply.attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (auto failure = transportError(reply, status)) {
        return std::unexpected(std::move(*failure));
    }

    const QByteArray payload = reply.readAll();
    if (status != 200) {
        return std::unexpected(errorFromUnaryResponse(status, payload, reply.errorString().toStdString()));
    }
    // A 200 that is not Protobuf is a captive portal / misrouted proxy, not our server.
    if (!hasContentType(reply, "application/proto")) {
        return std::unexpected(ApiError{.kind = ApiErrorKind::Decoding,
                                        .httpStatus = status,
                                        .message = "Unexpected response content-type: " +
                                                   reply.header(QNetworkRequest::ContentTypeHeader)
                                                       .toString()
                                                       .toStdString()});
    }
    if (reply.error() != QNetworkReply::NoError) {
        // Headers arrived but the body was cut off.
        return std::unexpected(
            ApiError{.kind = ApiErrorKind::Transport, .httpStatus = status, .message = reply.errorString().toStdString()});
    }
    return payload;
}

void completeUnary(const std::shared_ptr<detail::CallState>& state, detail::UnaryCall& call, RawResult result) {
    if (state->finished) {
        return;
    }
    state->finished = true;
    state->fail = nullptr;
    call.done(std::move(result));
}

void completeStream(const std::shared_ptr<detail::CallState>& state, detail::StreamCall& stream,
                    ApiResult<void> result) {
    if (state->finished) {
        return;
    }
    state->finished = true;
    state->fail = nullptr;
    stream.onDone(std::move(result));
}

/// Feeds bytes to the stream's reader and dispatches every complete frame.
/// Completes the call (and aborts the reply) on the end-of-stream frame or on
/// any protocol violation.
void consumeStreamBytes(const std::shared_ptr<detail::CallState>& state, detail::StreamCall& stream,
                        QNetworkReply& reply, const QByteArray& bytes) {
    stream.reader.feed(bytes);
    while (!state->finished) {
        auto frame = stream.reader.next();
        if (!frame) {
            completeStream(state, stream,
                           std::unexpected(ApiError{.kind = ApiErrorKind::Decoding, .httpStatus = 200, .message = frame.error()}));
            reply.abort();
            return;
        }
        if (!*frame) {
            return; // need more bytes
        }
        if ((*frame)->isCompressed()) {
            completeStream(state, stream,
                           std::unexpected(ApiError{.kind = ApiErrorKind::Decoding,
                                                    .httpStatus = 200,
                                                    .message = "Compressed stream messages are not supported"}));
            reply.abort();
            return;
        }
        if ((*frame)->isEndStream()) {
            completeStream(state, stream, parseEndStream((*frame)->payload));
            return;
        }
        if (auto handled = stream.onMessage(std::move((*frame)->payload)); !handled) {
            completeStream(state, stream, std::unexpected(std::move(handled.error())));
            reply.abort();
            return;
        }
    }
}

} // namespace

// --- CallHandle ---------------------------------------------------------------

void CallHandle::cancel() const {
    if (!state_ || state_->finished || state_->cancelled) {
        return;
    }
    state_->cancelled = true;
    if (state_->reply) {
        state_->reply->abort(); // finished() fires synchronously and completes with Cancelled
        return;
    }
    if (state_->fail) {
        state_->fail(cancelledError());
    }
}

bool CallHandle::isActive() const noexcept {
    return state_ && !state_->finished;
}

// --- ConnectClient ------------------------------------------------------------

ConnectClient::ConnectClient(ClientConfig config, std::shared_ptr<const AuthInterceptor> interceptor, QObject* parent)
    : QObject(parent), config_(std::move(config)), interceptor_(std::move(interceptor)) {
    // Replies are deleteLater()'d after finished(), so the slots below may still read them.
    network_.setAutoDeleteReplies(true);
    // A redirect would replay the POST (and possibly the bearer token) somewhere the
    // caller did not name; surface it as an error instead.
    network_.setRedirectPolicy(QNetworkRequest::ManualRedirectPolicy);

    // Certificate errors are only logged, never answered with ignoreSslErrors():
    // Qt then fails the request (SslHandshakeFailedError -> ApiErrorKind::Transport).
    // Only error descriptions and the host are logged, no request data.
    connect(&network_, &QNetworkAccessManager::sslErrors, this,
            [](QNetworkReply* reply, const QList<QSslError>& errors) {
                const std::string host = reply != nullptr ? reply->url().host().toStdString() : std::string{};
                for (const QSslError& error : errors) {
                    AedstLogger::error("TLS certificate validation failed for {}: {}", host,
                                       loggable(error.errorString().toStdString()));
                }
            });

    auto setup = buildTlsSetup(config_);
    if (!setup) {
        configError_ = std::move(setup.error());
        AedstLogger::error("API client is disabled, every call will fail: {}", loggable(configError_->message));
        return;
    }
    sslConfig_ = std::move(setup->ssl);
    pinnedSpki_ = std::move(setup->pins);
}

ApiResult<void> validateClientConfig(const ClientConfig& config) {
    if (auto setup = buildTlsSetup(config); !setup) {
        return std::unexpected(std::move(setup.error()));
    }
    return {};
}

const QUrl& ConnectClient::baseUrl() const noexcept {
    return config_.baseUrl;
}

void ConnectClient::prewarm() {
    if (configError_) {
        return;
    }
    const bool secure = config_.baseUrl.scheme() == QLatin1String("https");
    const auto port = static_cast<quint16>(config_.baseUrl.port(secure ? 443 : 80));
    if (secure) {
        network_.connectToHostEncrypted(config_.baseUrl.host(), port, sslConfig_);
    } else {
        network_.connectToHost(config_.baseUrl.host(), port);
    }
}

CallHandle ConnectClient::failAsync(ApiError error, RawCompletion done) {
    auto state = std::make_shared<detail::CallState>();
    QTimer::singleShot(0, this, [state, error = std::move(error), done = std::move(done)]() mutable {
        if (state->finished) {
            return;
        }
        state->finished = true;
        done(std::unexpected(std::move(error)));
    });
    return CallHandle{state};
}

CallHandle ConnectClient::unaryRaw(const MethodInfo& method, QByteArray body, const CallOptions& options,
                                   RawCompletion done) {
    if (configError_) {
        return failAsync(*configError_, std::move(done));
    }
    auto state = std::make_shared<detail::CallState>();
    auto call = std::make_shared<detail::UnaryCall>();
    call->correlationId = callCorrelationId();
    call->url = endpointUrl(config_.baseUrl, method.path);
    call->path = std::string(method.path);
    call->body = std::move(body);
    call->auth = method.auth;
    call->retry = method.idempotency == Idempotency::Idempotent ? options.retry.value_or(config_.retry)
                                                                : RetryPolicy::none();
    call->timeout = options.timeout.value_or(config_.defaultTimeout);
    call->done = std::move(done);

    state->fail = [weak = std::weak_ptr<detail::CallState>(state), call](ApiError error) {
        if (const auto locked = weak.lock()) {
            completeUnary(locked, *call, std::unexpected(std::move(error)));
        }
    };

    runUnaryAttempt(state, call, 1);
    return CallHandle{state};
}

void ConnectClient::runUnaryAttempt(const std::shared_ptr<detail::CallState>& state,
                                    const std::shared_ptr<detail::UnaryCall>& call, int attempt) {
    if (state->finished) {
        return;
    }
    if (state->cancelled) {
        completeUnary(state, *call, std::unexpected(cancelledError()));
        return;
    }

    QNetworkRequest request(call->url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/proto"));
    request.setRawHeader("Connect-Protocol-Version", "1");
    if (call->timeout.count() > 0) {
        request.setRawHeader("Connect-Timeout-Ms", QByteArray::number(static_cast<qlonglong>(call->timeout.count())));
    }
    if (interceptor_) {
        interceptor_->apply(request, call->auth); // per attempt: picks up a rotated token
    }
    hardenRequest(request, sslConfig_, call->correlationId);
    {
        const core::CorrelationScope scope(call->correlationId);
        AedstLogger::debug("POST {} (attempt {}/{})", call->path, attempt, call->retry.maxAttempts);
    }

    QNetworkReply* reply = network_.post(request, call->body);
    state->reply = reply;
    watchHandshake(reply, pinnedSpki_);
    armDeadline(reply, call->timeout);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, state, call, attempt]() { onUnaryFinished(*reply, state, call, attempt); });
}

void ConnectClient::onUnaryFinished(QNetworkReply& reply, const std::shared_ptr<detail::CallState>& state,
                                    const std::shared_ptr<detail::UnaryCall>& call, int attempt) {
    state->reply.clear();
    pinViolation(reply, pinnedSpki_);
    RawResult result = parseUnaryReply(reply);

    if (state->cancelled) {
        completeUnary(state, *call, std::unexpected(cancelledError()));
        return;
    }
    if (!result) {
        // A TLS failure is a verdict on the peer, not a blip: never retry it.
        const bool tlsFailure =
            reply.property(kPinFailedProperty).toBool() || reply.error() == QNetworkReply::SslHandshakeFailedError;
        const ApiError& error = result.error();
        // Logging only: the completion below must not run inside the scope, or
        // calls it starts would silently inherit this call's id.
        const core::CorrelationScope scope(call->correlationId);
        if (!tlsFailure && attempt < call->retry.maxAttempts && error.isRetryable()) {
            const auto delay = call->retry.delayBefore(attempt + 1, QRandomGenerator::global()->generateDouble());
            AedstLogger::warn("{} failed (http={} code={} message={}); retrying in {} ms", call->path,
                              error.httpStatus, loggable(error.connectCode), loggable(error.message), delay.count());
            QTimer::singleShot(delay, this, [this, state, call, attempt]() { runUnaryAttempt(state, call, attempt + 1); });
            return;
        }
        AedstLogger::warn("{} failed: http={} code={} message={}", call->path, error.httpStatus,
                          loggable(error.connectCode), loggable(error.message));
    }
    completeUnary(state, *call, std::move(result));
}

CallHandle ConnectClient::serverStreamRaw(const MethodInfo& method, QByteArray body, const CallOptions& options,
                                          RawMessageHandler onMessage, ApiCompletion<void> onDone) {
    if (configError_) {
        return failAsync(*configError_, [onDone = std::move(onDone)](RawResult result) {
            onDone(std::unexpected(std::move(result.error())));
        });
    }
    auto state = std::make_shared<detail::CallState>();
    auto stream = std::make_shared<detail::StreamCall>();
    stream->onMessage = std::move(onMessage);
    stream->onDone = std::move(onDone);

    state->fail = [weak = std::weak_ptr<detail::CallState>(state), stream](ApiError error) {
        if (const auto locked = weak.lock()) {
            completeStream(locked, *stream, std::unexpected(std::move(error)));
        }
    };

    // No default deadline: a stream is expected to outlive the unary timeout.
    const std::chrono::milliseconds timeout = options.timeout.value_or(std::chrono::milliseconds{0});

    QNetworkRequest request(endpointUrl(config_.baseUrl, method.path));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/connect+proto"));
    request.setRawHeader("Connect-Protocol-Version", "1");
    request.setRawHeader("Connect-Accept-Encoding", "identity"); // no per-message compression
    request.setRawHeader("Accept-Encoding", "identity");         // and no intermediary re-encoding
    if (timeout.count() > 0) {
        request.setRawHeader("Connect-Timeout-Ms", QByteArray::number(static_cast<qlonglong>(timeout.count())));
    }
    if (interceptor_) {
        interceptor_->apply(request, method.auth);
    }
    const std::string correlationId = callCorrelationId();
    hardenRequest(request, sslConfig_, correlationId);
    {
        const core::CorrelationScope scope(correlationId);
        AedstLogger::debug("POST {} (server stream)", std::string(method.path));
    }

    QNetworkReply* reply = network_.post(request, envelope::encode(0, body));
    state->reply = reply;
    watchHandshake(reply, pinnedSpki_);
    armDeadline(reply, timeout);

    connect(reply, &QNetworkReply::readyRead, this, [reply, state, stream, pins = pinnedSpki_]() {
        if (state->finished) {
            reply->readAll();
            return;
        }
        if (pinViolation(*reply, pins)) {
            return; // the abort inside completes the call through finished()
        }
        // Non-200 bodies are a plain JSON error, not envelopes: wait for finished().
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
            return;
        }
        if (!stream->contentTypeChecked) {
            stream->contentTypeChecked = true;
            if (!hasContentType(*reply, "application/connect+proto")) {
                completeStream(state, *stream,
                               std::unexpected(ApiError{.kind = ApiErrorKind::Decoding,
                                                        .httpStatus = 200,
                                                        .message = "Unexpected stream content-type"}));
                reply->abort();
                return;
            }
        }
        consumeStreamBytes(state, *stream, *reply, reply->readAll());
    });

    connect(reply, &QNetworkReply::finished, this, [reply, state, stream, pins = pinnedSpki_]() {
        state->reply.clear();
        if (state->finished) {
            return;
        }
        if (state->cancelled) {
            completeStream(state, *stream, std::unexpected(cancelledError()));
            return;
        }
        pinViolation(*reply, pins);
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (auto failure = transportError(*reply, status)) {
            completeStream(state, *stream, std::unexpected(std::move(*failure)));
            return;
        }
        const QByteArray rest = reply->readAll();
        if (status != 200) {
            completeStream(state, *stream,
                           std::unexpected(errorFromUnaryResponse(status, rest, reply->errorString().toStdString())));
            return;
        }
        if (!rest.isEmpty()) {
            consumeStreamBytes(state, *stream, *reply, rest);
        }
        completeStream(state, *stream,
                       std::unexpected(ApiError{.kind = ApiErrorKind::Transport,
                                                .httpStatus = status,
                                                .message = "Stream ended without an end-of-stream message"}));
    });

    return CallHandle{state};
}

} // namespace aedst::network
