#include "aedst/network/connect_envelope.hpp"

#include <utility>

namespace aedst::network::envelope {
namespace {

/// Compact once this much consumed prefix has piled up, so a long stream of
/// small frames does not memmove the buffer on every next().
constexpr std::size_t kCompactThresholdBytes = 64U * 1024U;

} // namespace

QByteArray encode(std::uint8_t flags, QByteArrayView payload) {
    const auto length = static_cast<std::uint32_t>(payload.size());
    QByteArray framed;
    framed.reserve(payload.size() + static_cast<qsizetype>(kHeaderSize));
    framed.append(static_cast<char>(flags));
    framed.append(static_cast<char>((length >> 24U) & 0xFFU));
    framed.append(static_cast<char>((length >> 16U) & 0xFFU));
    framed.append(static_cast<char>((length >> 8U) & 0xFFU));
    framed.append(static_cast<char>(length & 0xFFU));
    framed.append(payload);
    return framed;
}

EnvelopeReader::EnvelopeReader(std::size_t maxPayloadBytes) noexcept : maxPayloadBytes_(maxPayloadBytes) {}

void EnvelopeReader::feed(QByteArrayView bytes) {
    buffer_.append(bytes);
}

std::expected<std::optional<Frame>, std::string> EnvelopeReader::next() {
    const std::size_t available = static_cast<std::size_t>(buffer_.size()) - offset_;
    if (available < kHeaderSize) {
        return std::optional<Frame>{};
    }

    const auto byteAt = [this](std::size_t index) {
        return static_cast<std::uint32_t>(static_cast<std::uint8_t>(buffer_[static_cast<qsizetype>(offset_ + index)]));
    };
    const auto flags = static_cast<std::uint8_t>(byteAt(0));
    const std::uint32_t length = (byteAt(1) << 24U) | (byteAt(2) << 16U) | (byteAt(3) << 8U) | byteAt(4);

    if (length > maxPayloadBytes_) {
        return std::unexpected("Envelope declares a " + std::to_string(length) + "-byte message; limit is " +
                               std::to_string(maxPayloadBytes_));
    }
    if (available < kHeaderSize + length) {
        return std::optional<Frame>{};
    }

    Frame frame{.flags = flags,
                .payload = buffer_.mid(static_cast<qsizetype>(offset_ + kHeaderSize), static_cast<qsizetype>(length))};
    offset_ += kHeaderSize + length;

    if (offset_ == static_cast<std::size_t>(buffer_.size())) {
        buffer_.clear();
        offset_ = 0;
    } else if (offset_ >= kCompactThresholdBytes) {
        buffer_.remove(0, static_cast<qsizetype>(offset_));
        offset_ = 0;
    }
    return std::optional<Frame>{std::move(frame)};
}

std::size_t EnvelopeReader::bufferedBytes() const noexcept {
    return static_cast<std::size_t>(buffer_.size()) - offset_;
}

} // namespace aedst::network::envelope
