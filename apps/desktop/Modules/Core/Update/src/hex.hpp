#pragma once

// Private to AedstUpdate.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace aedst::update::detail {

/// Decodes lowercase-or-uppercase hex; nullopt on odd length or bad characters.
[[nodiscard]] inline std::optional<std::vector<std::uint8_t>> decodeHex(std::string_view hex) {
    if (hex.size() % 2 != 0) {
        return std::nullopt;
    }
    const auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f') {
            return c - 'a' + 10;
        }
        if (c >= 'A' && c <= 'F') {
            return c - 'A' + 10;
        }
        return -1;
    };
    std::vector<std::uint8_t> out;
    out.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        const int hi = nibble(hex[i]);
        const int lo = nibble(hex[i + 1]);
        if (hi < 0 || lo < 0) {
            return std::nullopt;
        }
        out.push_back(static_cast<std::uint8_t>(hi * 16 + lo));
    }
    return out;
}

} // namespace aedst::update::detail
