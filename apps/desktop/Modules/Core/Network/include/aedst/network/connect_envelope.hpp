#pragma once

#include <QByteArray>
#include <QByteArrayView>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>

/// Connect streaming envelope (https://connectrpc.com/docs/protocol#streaming-request):
///
///     [ flags : 1 byte ][ length : 4 bytes, big-endian ][ payload : length bytes ]
///
/// Pure byte-level codec - no Qt event loop, no network - so it is tested with the
/// spec's golden vectors.
namespace aedst::network::envelope {

inline constexpr std::size_t kHeaderSize = 5;
inline constexpr std::uint8_t kFlagCompressed = 0x01;
inline constexpr std::uint8_t kFlagEndStream = 0x02;
/// Reader default; mirrors connect-go's default read limit order of magnitude.
inline constexpr std::size_t kDefaultMaxPayloadBytes = 4U * 1024U * 1024U;

struct Frame {
    std::uint8_t flags{0};
    QByteArray payload;

    [[nodiscard]] bool isCompressed() const noexcept { return (flags & kFlagCompressed) != 0; }

    [[nodiscard]] bool isEndStream() const noexcept { return (flags & kFlagEndStream) != 0; }

    friend bool operator==(const Frame&, const Frame&) = default;
};

/// Frames one message. `flags` is 0 for a plain message, kFlagEndStream for the
/// terminating frame (only servers send that one).
[[nodiscard]] QByteArray encode(std::uint8_t flags, QByteArrayView payload);

/// Incremental decoder: feed() whatever bytes the socket delivered (any split,
/// including mid-header), then drain next() until it yields no frame.
class EnvelopeReader final {
public:
    explicit EnvelopeReader(std::size_t maxPayloadBytes = kDefaultMaxPayloadBytes) noexcept;

    void feed(QByteArrayView bytes);

    /// A complete frame; `std::nullopt` when more bytes are needed; an error string
    /// when a frame declares a payload larger than the limit (the stream is then
    /// unusable - abort it).
    [[nodiscard]] std::expected<std::optional<Frame>, std::string> next();

    /// Bytes received but not yet returned as frames (a partial frame at stream end
    /// means the peer cut the stream short).
    [[nodiscard]] std::size_t bufferedBytes() const noexcept;

private:
    QByteArray buffer_;
    std::size_t offset_{0};
    std::size_t maxPayloadBytes_;
};

} // namespace aedst::network::envelope
