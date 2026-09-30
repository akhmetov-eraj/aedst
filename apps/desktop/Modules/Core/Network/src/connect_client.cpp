#include "aedst/network/connect_client.hpp"

#include "aedst/core/logger.hpp"
#include "aedst/network/auth_interceptor.hpp"
#include "aedst/network/connect_envelope.hpp"
#include "aedst/network/connect_error.hpp"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRandomGenerator>
#include <QString>
#include <QTimer>

#include <limits>
#include <string>
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

[[nodiscard]] ApiError cancelledError() {
    return {.kind = ApiErrorKind::Cancelled, .httpStatus = 0, .message = "Call cancelled"};
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
    if (reply.property(kTimedOutProperty).toBool() || netError == QNetworkReply::TimeoutError) {
        return ApiError{.kind = ApiErrorKind::Timeout, .httpStatus = status, .message = "Request timed out"};
    }
    if (netError == QNetworkReply::OperationCanceledError) {
        return cancelledError();
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
}

const QUrl& ConnectClient::baseUrl() const noexcept {
    return config_.baseUrl;
}

void ConnectClient::prewarm() {
    const bool secure = config_.baseUrl.scheme() == QLatin1String("https");
    const auto port = static_cast<quint16>(config_.baseUrl.port(secure ? 443 : 80));
    if (secure) {
        network_.connectToHostEncrypted(config_.baseUrl.host(), port);
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
    auto state = std::make_shared<detail::CallState>();
    auto call = std::make_shared<detail::UnaryCall>();
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
    AedstLogger::debug("POST {} (attempt {}/{})", call->path, attempt, call->retry.maxAttempts);

    QNetworkReply* reply = network_.post(request, call->body);
    state->reply = reply;
    armDeadline(reply, call->timeout);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, state, call, attempt]() { onUnaryFinished(*reply, state, call, attempt); });
}

void ConnectClient::onUnaryFinished(QNetworkReply& reply, const std::shared_ptr<detail::CallState>& state,
                                    const std::shared_ptr<detail::UnaryCall>& call, int attempt) {
    state->reply.clear();
    RawResult result = parseUnaryReply(reply);

    if (state->cancelled) {
        completeUnary(state, *call, std::unexpected(cancelledError()));
        return;
    }
    if (!result) {
        const ApiError& error = result.error();
        if (attempt < call->retry.maxAttempts && error.isRetryable()) {
            const auto delay = call->retry.delayBefore(attempt + 1, QRandomGenerator::global()->generateDouble());
            AedstLogger::warn("{} failed (http={} code={} message={}); retrying in {} ms", call->path,
                              error.httpStatus, error.connectCode, error.message, delay.count());
            QTimer::singleShot(delay, this, [this, state, call, attempt]() { runUnaryAttempt(state, call, attempt + 1); });
            return;
        }
        AedstLogger::warn("{} failed: http={} code={} message={}", call->path, error.httpStatus, error.connectCode,
                          error.message);
    }
    completeUnary(state, *call, std::move(result));
}

CallHandle ConnectClient::serverStreamRaw(const MethodInfo& method, QByteArray body, const CallOptions& options,
                                          RawMessageHandler onMessage, ApiCompletion<void> onDone) {
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
    AedstLogger::debug("POST {} (server stream)", std::string(method.path));

    QNetworkReply* reply = network_.post(request, envelope::encode(0, body));
    state->reply = reply;
    armDeadline(reply, timeout);

    connect(reply, &QNetworkReply::readyRead, this, [reply, state, stream]() {
        if (state->finished) {
            reply->readAll();
            return;
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

    connect(reply, &QNetworkReply::finished, this, [reply, state, stream]() {
        state->reply.clear();
        if (state->finished) {
            return;
        }
        if (state->cancelled) {
            completeStream(state, *stream, std::unexpected(cancelledError()));
            return;
        }
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
