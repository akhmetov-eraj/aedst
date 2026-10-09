#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace aedst::network {

/// Name of the header every first-party client sends so the server can count requests per platform and enforce
/// a minimum version (ADR-0009, apps/server/internal/platform/httpserver/clientversion.go).
inline constexpr std::string_view kClientHeaderName = "X-Aedst-Client";

/// Platform token of this app in the header value `<platform>/<semver>`.
inline constexpr std::string_view kClientPlatform = "desktop";

namespace detail {

[[nodiscard]] constexpr bool isDigits(std::string_view text) noexcept {
    if (text.empty()) {
        return false;
    }
    for (const char c : text) {
        if (c < '0' || c > '9') {
            return false;
        }
    }
    return true;
}

} // namespace detail

/// "MAJOR.MINOR.PATCH" with an optional "-prerelease" and "+build" suffix (semver 2.0.0, without validating the
/// identifier grammar of the suffixes: the server parses those strictly and lets a request through when it cannot).
[[nodiscard]] constexpr bool looksLikeSemver(std::string_view version) noexcept {
    const std::size_t suffix = version.find_first_of("-+");
    std::string_view core = version.substr(0, suffix);
    if (suffix != std::string_view::npos && suffix + 1 >= version.size()) {
        return false; // "1.2.3-" / "1.2.3+"
    }
    for (int part = 0; part < 3; ++part) {
        const std::size_t dot = core.find('.');
        const std::string_view number = core.substr(0, dot);
        if (!detail::isDigits(number)) {
            return false;
        }
        if (part == 2) {
            return dot == std::string_view::npos;
        }
        if (dot == std::string_view::npos) {
            return false;
        }
        core.remove_prefix(dot + 1);
    }
    return false;
}

/// `desktop/<version>` for the X-Aedst-Client header, or an empty string when `version` is not a semantic version
/// (the header is then omitted: the server lets unidentified clients through, a malformed value would only add noise).
[[nodiscard]] inline std::string makeClientIdentifier(std::string_view version) {
    if (version.starts_with('v')) {
        version.remove_prefix(1);
    }
    if (!looksLikeSemver(version)) {
        return {};
    }
    std::string value(kClientPlatform);
    value += '/';
    value += version;
    return value;
}

} // namespace aedst::network
