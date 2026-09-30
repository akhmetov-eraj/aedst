#include "aedst/network/connect_error.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>

#include <utility>

namespace aedst::network {
namespace {

[[nodiscard]] ApiError fromErrorObject(int httpStatus, const QJsonObject& error, const std::string& fallbackMessage) {
    const std::string code = error.value(QStringLiteral("code")).toString().toStdString();
    std::string message = error.value(QStringLiteral("message")).toString().toStdString();
    if (message.empty()) {
        message = fallbackMessage.empty() ? code : fallbackMessage;
    }
    return ApiError::fromConnectResponse(httpStatus, code, std::move(message));
}

} // namespace

ApiError errorFromUnaryResponse(int httpStatus, const QByteArray& body, const std::string& fallbackMessage) {
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (document.isObject()) {
        return fromErrorObject(httpStatus, document.object(), fallbackMessage);
    }
    const std::string message = fallbackMessage.empty() ? "HTTP " + std::to_string(httpStatus) : fallbackMessage;
    return ApiError::fromConnectResponse(httpStatus, "", message);
}

std::expected<void, ApiError> parseEndStream(const QByteArray& payload) {
    if (payload.isEmpty()) {
        return {};
    }
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return std::unexpected(ApiError{.kind = ApiErrorKind::Decoding,
                                        .httpStatus = 200,
                                        .message = "Malformed end-of-stream message: " +
                                                   parseError.errorString().toStdString()});
    }
    const QJsonValue error = document.object().value(QStringLiteral("error"));
    if (!error.isObject()) {
        return {};
    }
    // Streaming errors travel inside an HTTP 200; the code alone classifies them.
    return std::unexpected(fromErrorObject(200, error.toObject(), ""));
}

} // namespace aedst::network
