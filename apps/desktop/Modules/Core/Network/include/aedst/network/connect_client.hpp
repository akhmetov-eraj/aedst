#pragma once

#include "aedst/network/api_error.hpp"
#include "aedst/network/call_options.hpp"
#include "aedst/network/method.hpp"

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>

#include <google/protobuf/message_lite.h>

#include <chrono>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

class QNetworkReply;

namespace aedst::network {

class AuthInterceptor;

namespace detail {
struct CallState;
struct UnaryCall;
struct StreamCall;
} // namespace detail

using RawResult = ApiResult<QByteArray>;
using RawCompletion = std::function<void(RawResult)>;
/// Receives one server-streamed message. Returning an error aborts the stream
/// and completes it with that error (e.g. the payload did not parse).
using RawMessageHandler = std::function<std::expected<void, ApiError>(QByteArray)>;

/// Cancels an in-flight call. Cheap to copy; safe to keep after the call
/// finished (then it does nothing). Call from the thread that owns the client.
class CallHandle final {
public:
    CallHandle() = default;

    /// The completion runs once, with ApiErrorKind::Cancelled, if the call was
    /// still in flight.
    void cancel() const;

    [[nodiscard]] bool isActive() const noexcept;

private:
    friend class ConnectClient;
    explicit CallHandle(std::shared_ptr<detail::CallState> state) noexcept : state_(std::move(state)) {}

    std::shared_ptr<detail::CallState> state_;
};

struct ClientConfig {
    QUrl baseUrl;
    /// Per-attempt deadline unless CallOptions::timeout overrides it.
    std::chrono::milliseconds defaultTimeout{std::chrono::seconds{15}};
    RetryPolicy retry{};
};

/// Connect-protocol client over Qt Network with Protobuf messages.
///
/// Implements the subset of https://connectrpc.com/docs/protocol the app needs:
///  - unary RPCs: `POST <base>/<package.Service>/<Method>`, `application/proto`,
///    `Connect-Protocol-Version: 1`, `Connect-Timeout-Ms`, Connect error model;
///  - server-streaming RPCs: `application/connect+proto` envelope framing.
/// Not implemented (no server RPC needs them, and QNetworkAccessManager cannot do
/// full-duplex HTTP): client/bidi streaming, GET requests for side-effect-free
/// methods, per-message compression (HTTP-level gzip/deflate is negotiated and
/// decoded transparently by Qt).
///
/// TLS, HTTP/2 (ALPN), proxies and the OS certificate store are Qt Network's
/// job - nothing here reimplements them. Everything runs on the owner thread's
/// event loop; completions are always delivered asynchronously, exactly once.
/// See docs/adr/0001-api-contract-client.md for why this is not gRPC C++.
class ConnectClient final : public QObject {
    Q_OBJECT

public:
    ConnectClient(ClientConfig config, std::shared_ptr<const AuthInterceptor> interceptor,
                  QObject* parent = nullptr);

    /// Typed unary call. Retried per the RetryPolicy only when `method` is Idempotent.
    template <typename Req, typename Res>
    CallHandle unary(const Method<Req, Res>& method, const Req& request,
                     std::type_identity_t<ApiCompletion<Res>> done, const CallOptions& options = {});

    /// Typed server-streaming call: `onMessage` per message, then `onDone` once
    /// (success = the server sent a clean end-of-stream). Never retried.
    template <typename Req, typename Res>
    CallHandle serverStream(const Method<Req, Res>& method, const Req& request,
                            std::type_identity_t<std::function<void(Res)>> onMessage,
                            std::type_identity_t<ApiCompletion<void>> onDone, const CallOptions& options = {});

    /// Untyped layer the templates sit on (already-serialized Protobuf bytes).
    CallHandle unaryRaw(const MethodInfo& method, QByteArray body, const CallOptions& options, RawCompletion done);
    CallHandle serverStreamRaw(const MethodInfo& method, QByteArray body, const CallOptions& options,
                               RawMessageHandler onMessage, ApiCompletion<void> onDone);

    /// Completes `done` with `error` on the next event-loop turn.
    CallHandle failAsync(ApiError error, RawCompletion done);

    /// Opens the TCP (+TLS, +HTTP/2 negotiation) connection ahead of the first call,
    /// so the login request does not pay the handshake. Best-effort, no callback.
    void prewarm();

    [[nodiscard]] const QUrl& baseUrl() const noexcept;

private:
    void runUnaryAttempt(const std::shared_ptr<detail::CallState>& state, const std::shared_ptr<detail::UnaryCall>& call,
                         int attempt);
    void onUnaryFinished(QNetworkReply& reply, const std::shared_ptr<detail::CallState>& state,
                         const std::shared_ptr<detail::UnaryCall>& call, int attempt);

    QNetworkAccessManager network_;
    ClientConfig config_;
    std::shared_ptr<const AuthInterceptor> interceptor_;
};

namespace detail {

[[nodiscard]] std::optional<QByteArray> serialize(const google::protobuf::MessageLite& message);

template <typename Res>
[[nodiscard]] ApiResult<Res> parseMessage(const QByteArray& bytes) {
    Res message;
    if (!message.ParseFromArray(bytes.constData(), static_cast<int>(bytes.size()))) {
        return std::unexpected(ApiError{
            .kind = ApiErrorKind::Decoding, .httpStatus = 200, .message = "Malformed " + message.GetTypeName()});
    }
    return message;
}

} // namespace detail

template <typename Req, typename Res>
CallHandle ConnectClient::unary(const Method<Req, Res>& method, const Req& request,
                                std::type_identity_t<ApiCompletion<Res>> done, const CallOptions& options) {
    static_assert(std::is_base_of_v<google::protobuf::MessageLite, Req>, "Req must be a generated Protobuf message");
    static_assert(std::is_base_of_v<google::protobuf::MessageLite, Res>, "Res must be a generated Protobuf message");

    auto body = detail::serialize(request);
    if (!body) {
        return failAsync(ApiError{.kind = ApiErrorKind::Internal,
                                  .httpStatus = 0,
                                  .message = "Cannot serialize " + request.GetTypeName()},
                         [done = std::move(done)](RawResult result) { done(std::unexpected(std::move(result.error()))); });
    }
    return unaryRaw(method.info, std::move(*body), options, [done = std::move(done)](RawResult raw) {
        if (!raw) {
            done(std::unexpected(std::move(raw.error())));
            return;
        }
        done(detail::parseMessage<Res>(*raw));
    });
}

template <typename Req, typename Res>
CallHandle ConnectClient::serverStream(const Method<Req, Res>& method, const Req& request,
                                       std::type_identity_t<std::function<void(Res)>> onMessage,
                                       std::type_identity_t<ApiCompletion<void>> onDone, const CallOptions& options) {
    static_assert(std::is_base_of_v<google::protobuf::MessageLite, Req>, "Req must be a generated Protobuf message");
    static_assert(std::is_base_of_v<google::protobuf::MessageLite, Res>, "Res must be a generated Protobuf message");

    auto body = detail::serialize(request);
    if (!body) {
        return failAsync(ApiError{.kind = ApiErrorKind::Internal,
                                  .httpStatus = 0,
                                  .message = "Cannot serialize " + request.GetTypeName()},
                         [onDone = std::move(onDone)](RawResult result) {
                             onDone(std::unexpected(std::move(result.error())));
                         });
    }
    return serverStreamRaw(
        method.info, std::move(*body), options,
        [onMessage = std::move(onMessage)](QByteArray raw) -> std::expected<void, ApiError> {
            auto message = detail::parseMessage<Res>(raw);
            if (!message) {
                return std::unexpected(std::move(message.error()));
            }
            onMessage(std::move(*message));
            return {};
        },
        std::move(onDone));
}

} // namespace aedst::network
