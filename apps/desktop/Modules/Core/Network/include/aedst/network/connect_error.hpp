#pragma once

#include "aedst/network/api_error.hpp"

#include <QByteArray>

#include <expected>
#include <string>

/// Connect error model (https://connectrpc.com/docs/protocol#error-end-stream):
/// JSON `{"code": "not_found", "message": "...", "details": [...]}`, with the
/// code taking precedence over the HTTP status.
namespace aedst::network {

/// Error for a unary response with a non-200 status. `fallbackMessage` is used
/// when the body is not a Connect error (proxy/LB pages, empty bodies).
[[nodiscard]] ApiError errorFromUnaryResponse(int httpStatus, const QByteArray& body,
                                              const std::string& fallbackMessage);

/// Parses a streaming end-of-stream frame payload: `{}` or `{"error": {...}, "metadata": {...}}`.
/// Success means the stream ended cleanly; otherwise the embedded error (or a
/// Decoding error if the payload is not valid JSON).
[[nodiscard]] std::expected<void, ApiError> parseEndStream(const QByteArray& payload);

} // namespace aedst::network
