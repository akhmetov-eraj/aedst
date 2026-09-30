#pragma once

#include "aedst/core/outcome.hpp"

#include <compare>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace aedst::update {

/// Semantic Version 2.0.0 (https://semver.org). Build metadata is kept but,
/// per the spec, never takes part in precedence.
struct SemVer {
    std::uint64_t major{0};
    std::uint64_t minor{0};
    std::uint64_t patch{0};
    /// Dot-separated pre-release identifiers, e.g. {"rc", "1"}.
    std::vector<std::string> prerelease;
    std::string buildMetadata;

    [[nodiscard]] std::string toString() const;
};

/// Strict parser: `MAJOR.MINOR.PATCH[-prerelease][+build]`, at most 64
/// characters, no leading `v`, no leading zeros in numeric identifiers, numeric
/// core parts of at most 18 digits.
[[nodiscard]] core::Outcome<SemVer> parseSemVer(std::string_view text);

/// Precedence per semver 2.0.0 section 11 (numeric < alphanumeric identifiers,
/// a pre-release sorts before its release, shorter identifier lists sort first
/// when all shared identifiers are equal). Build metadata is ignored.
[[nodiscard]] std::strong_ordering compareSemVer(const SemVer& lhs, const SemVer& rhs);

} // namespace aedst::update
