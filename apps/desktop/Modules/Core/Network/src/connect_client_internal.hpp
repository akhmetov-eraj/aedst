#pragma once

// Private to the Network module (lives in src/, not installed or exported): the pieces of
// ConnectClient that are shared between connect_client.cpp, connect_client_stream.cpp,
// connect_client_tls.cpp and connect_client_support.cpp. Moved out of one translation unit
// without behaviour change.

#include "aedst/core/guarded.hpp"
#include "aedst/core/secure_memory.hpp"
#include "aedst/network/connect_client.hpp"
#include "aedst/network/connect_envelope.hpp"

#include <QByteArray>
#include <QList>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QSslConfiguration>
#include <QUrl>

#include <chrono>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace aedst::network {

namespace detail {

/// Lifecycle shared between a CallHandle and the in-flight machinery.
struct CallState {
    bool cancelled{false};
    bool finished{false};
    /// Cancelled by an owner that is going away: the completion must not run at all.
    bool silent{false};
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
    /// Sent as `Idempotency-Key` on every attempt; empty = none (validated before the call is created).
    std::string idempotencyKey;
    RawCompletion done;
    /// The bearer token this call sent last, handed back to the refresher on a rejection.
    core::SecretString sentToken;
    /// A rejected call is replayed after a refresh at most once.
    bool refreshedAfterRejection{false};
};

struct StreamCall {
    envelope::EnvelopeReader reader;
    RawMessageHandler onMessage;
    ApiCompletion<void> onDone;
    bool contentTypeChecked{false};

    // Everything needed to (re)open the request after a token refresh.
    std::string path;
    QUrl url;
    QByteArray body;
    AuthRequirement auth{AuthRequirement::Required};
    std::chrono::milliseconds timeout{0};
    std::string correlationId;
    core::SecretString sentToken;
    bool refreshedAfterRejection{false};
};

} // namespace detail

namespace internal {

inline constexpr const char* kTimedOutProperty = "aedstTimedOut";
inline constexpr const char* kPinFailedProperty = "aedstPinFailed";
inline constexpr const char* kTooLargeProperty = "aedstTooLarge";

struct TlsSetup {
    QSslConfiguration ssl;
    QList<QByteArray> pins; // raw SHA-256 digests of SubjectPublicKeyInfo DER
};

// --- connect_client_support.cpp ---
[[nodiscard]] ApiError cancelledError();
[[nodiscard]] std::string loggable(std::string_view text);
[[nodiscard]] std::string callCorrelationId();
[[nodiscard]] std::unexpected<ApiError> invalidConfig(std::string message);
/// TLS policy + the headers every request carries: X-Correlation-Id, and X-Aedst-Client when `clientIdentifier`
/// is not empty (both unary and streaming calls).
void hardenRequest(QNetworkRequest& request, const QSslConfiguration& ssl, const std::string& correlationId,
                   const std::string& clientIdentifier);
[[nodiscard]] QUrl endpointUrl(const QUrl& base, std::string_view path);
void armDeadline(QNetworkReply* reply, std::chrono::milliseconds timeout);
[[nodiscard]] bool hasContentType(const QNetworkReply& reply, const char* expected);
[[nodiscard]] std::optional<ApiError> transportError(const QNetworkReply& reply, int status);
[[nodiscard]] RawResult parseUnaryReply(QNetworkReply& reply);
void completeUnary(const std::shared_ptr<detail::CallState>& state, detail::UnaryCall& call, RawResult result);
void completeStream(const std::shared_ptr<detail::CallState>& state, detail::StreamCall& stream,
                    ApiResult<void> result);
void consumeStreamBytes(const std::shared_ptr<detail::CallState>& state, detail::StreamCall& stream,
                        QNetworkReply& reply, const QByteArray& bytes);

// --- connect_client_tls.cpp ---
[[nodiscard]] ApiError pinError();
[[nodiscard]] ApiResult<TlsSetup> buildTlsSetup(const ClientConfig& config);
bool pinViolation(QNetworkReply& reply, const QList<QByteArray>& pins);
void watchHandshake(QNetworkReply* reply, const QList<QByteArray>& pins);

/// Every call into caller-supplied code goes through the shared guard in Aedst::Core (an exception is logged and
/// swallowed instead of terminating the Qt event loop).
using aedst::core::invokeGuarded;

} // namespace internal

} // namespace aedst::network
