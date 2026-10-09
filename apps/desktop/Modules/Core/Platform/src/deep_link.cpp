#include "aedst/platform/deep_link.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace aedst::platform {
namespace {

using core::Error;
using core::fail;

constexpr std::string_view kSchemePrefix = "aedst://";
constexpr std::string_view kSchemeShort = "aedst:";

/// host -> allowed path shape. A non-empty `fixedSegment` means the (only)
/// segment must equal it; empty means any identifier-shaped segment.
struct RouteRule {
    std::string_view host;
    std::size_t minSegments;
    std::size_t maxSegments;
    std::string_view fixedSegment;
};

/// Hosts that are not screens of a feature, so no active route can declare them: the OIDC redirect and the
/// settings entry. Every other host comes from the active routes (see parseDeepLink).
constexpr std::array<RouteRule, 2> kFixedRules{{
    {"auth", 1, 1, "callback"},
    {"settings", 0, 0, ""},
}};

char asciiLower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return static_cast<char>(c - 'A' + 'a');
    }
    return c;
}

bool equalsIgnoreCase(std::string_view text, std::string_view lowerTarget) {
    if (text.size() != lowerTarget.size()) {
        return false;
    }
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (asciiLower(text[i]) != lowerTarget[i]) {
            return false;
        }
    }
    return true;
}

bool isRawForbidden(char c) {
    const auto u = static_cast<unsigned char>(c);
    return u < 0x20U || u == 0x7FU || u >= 0x80U || c == ' ' || c == '\\';
}

bool isAlnum(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

bool isUnreserved(char c) {
    return isAlnum(c) || c == '-' || c == '.' || c == '_' || c == '~';
}

int hexValue(char c) {
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
}

bool isValidUtf8(std::string_view text) {
    std::size_t i = 0;
    while (i < text.size()) {
        const auto lead = static_cast<unsigned char>(text[i]);
        if (lead < 0x80U) {
            ++i;
            continue;
        }
        std::size_t extra = 0;
        std::uint32_t codePoint = 0;
        std::uint32_t minimum = 0;
        if (lead >= 0xC2U && lead <= 0xDFU) {
            extra = 1;
            codePoint = lead & 0x1FU;
            minimum = 0x80U;
        } else if (lead >= 0xE0U && lead <= 0xEFU) {
            extra = 2;
            codePoint = lead & 0x0FU;
            minimum = 0x800U;
        } else if (lead >= 0xF0U && lead <= 0xF4U) {
            extra = 3;
            codePoint = lead & 0x07U;
            minimum = 0x10000U;
        } else {
            return false;
        }
        if (i + extra >= text.size()) {
            return false;
        }
        for (std::size_t k = 1; k <= extra; ++k) {
            const auto cont = static_cast<unsigned char>(text[i + k]);
            if ((cont & 0xC0U) != 0x80U) {
                return false;
            }
            codePoint = (codePoint << 6U) | (cont & 0x3FU);
        }
        if (codePoint < minimum || codePoint > 0x10FFFFU || (codePoint >= 0xD800U && codePoint <= 0xDFFFU)) {
            return false;
        }
        i += extra + 1;
    }
    return true;
}

/// Decodes %XX escapes; nullopt on malformed escapes, decoded control
/// characters or invalid UTF-8.
std::optional<std::string> percentDecode(std::string_view in) {
    std::string out;
    out.reserve(in.size());
    for (std::size_t i = 0; i < in.size(); ++i) {
        char c = in[i];
        if (c == '%') {
            if (i + 2 >= in.size()) {
                return std::nullopt;
            }
            const int hi = hexValue(in[i + 1]);
            const int lo = hexValue(in[i + 2]);
            if (hi < 0 || lo < 0) {
                return std::nullopt;
            }
            c = static_cast<char>(hi * 16 + lo);
            i += 2;
        }
        const auto u = static_cast<unsigned char>(c);
        if (u < 0x20U || u == 0x7FU) {
            return std::nullopt;
        }
        out.push_back(c);
    }
    if (!isValidUtf8(out)) {
        return std::nullopt;
    }
    return out;
}

bool isValidKey(std::string_view key) {
    if (key.empty() || key.size() > kMaxDeepLinkKeyLength) {
        return false;
    }
    if (key.front() < 'a' || key.front() > 'z') {
        return false;
    }
    for (const char c : key) {
        const bool lower = c >= 'a' && c <= 'z';
        const bool digit = c >= '0' && c <= '9';
        if (!lower && !digit && c != '_') {
            return false;
        }
    }
    return true;
}

bool isValidSegment(std::string_view segment) {
    if (segment.empty() || segment.size() > kMaxDeepLinkSegmentLength || segment == "." || segment == "..") {
        return false;
    }
    for (const char c : segment) {
        if (!isUnreserved(c)) {
            return false;
        }
    }
    return true;
}

std::vector<std::string_view> split(std::string_view text, char separator) {
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    while (true) {
        const std::size_t pos = text.find(separator, start);
        if (pos == std::string_view::npos) {
            parts.push_back(text.substr(start));
            return parts;
        }
        parts.push_back(text.substr(start, pos - start));
        start = pos + 1;
    }
}

/// A fixed rule, else a route-derived one: the host is a link prefix of an active route and takes at most one
/// identifier segment.
std::optional<RouteRule> findRule(std::string_view host, std::span<const std::string> routeHosts) {
    for (const RouteRule& rule : kFixedRules) {
        if (rule.host == host) {
            return rule;
        }
    }
    for (const std::string& routeHost : routeHosts) {
        if (routeHost == host) {
            return RouteRule{.host = routeHost, .minSegments = 0, .maxSegments = 1, .fixedSegment = {}};
        }
    }
    return std::nullopt;
}

core::Outcome<std::string> parseRoute(std::string_view pathPart, std::span<const std::string> routeHosts) {
    if (pathPart.ends_with('/')) {
        pathPart.remove_suffix(1);
    }
    const std::vector<std::string_view> parts = split(pathPart, '/');
    std::string host;
    for (const char c : parts.front()) {
        host.push_back(asciiLower(c));
    }
    const std::optional<RouteRule> rule = findRule(host, routeHosts);
    if (!rule) {
        return fail(Error::invalidInput("deep link: host not allowed"));
    }
    const std::size_t segmentCount = parts.size() - 1;
    if (segmentCount < rule->minSegments || segmentCount > rule->maxSegments) {
        return fail(Error::invalidInput("deep link: path not allowed"));
    }
    std::string route = host;
    for (std::size_t i = 1; i < parts.size(); ++i) {
        if (!isValidSegment(parts[i])) {
            return fail(Error::invalidInput("deep link: invalid path segment"));
        }
        if (!rule->fixedSegment.empty() && parts[i] != rule->fixedSegment) {
            return fail(Error::invalidInput("deep link: path not allowed"));
        }
        route.push_back('/');
        route.append(parts[i]);
    }
    return route;
}

using ParamList = std::vector<std::pair<std::string, std::string>>;

core::Outcome<ParamList> parseQuery(std::string_view query) {
    ParamList params;
    if (query.empty()) {
        return params;
    }
    for (const std::string_view pair : split(query, '&')) {
        if (params.size() >= kMaxDeepLinkParams) {
            return fail(Error::invalidInput("deep link: too many parameters"));
        }
        const std::size_t eq = pair.find('=');
        const std::string_view key = pair.substr(0, eq);
        const std::string_view rawValue = eq == std::string_view::npos ? std::string_view{} : pair.substr(eq + 1);
        if (!isValidKey(key)) {
            return fail(Error::invalidInput("deep link: invalid parameter name"));
        }
        for (const auto& existing : params) {
            if (existing.first == key) {
                return fail(Error::invalidInput("deep link: duplicate parameter"));
            }
        }
        for (const char c : rawValue) {
            if (!isUnreserved(c) && c != '%') {
                return fail(Error::invalidInput("deep link: invalid character in parameter value"));
            }
        }
        std::optional<std::string> decoded = percentDecode(rawValue);
        if (!decoded) {
            return fail(Error::invalidInput("deep link: invalid percent-encoding"));
        }
        if (decoded->size() > kMaxDeepLinkValueLength) {
            return fail(Error::invalidInput("deep link: parameter value too long"));
        }
        params.emplace_back(std::string{key}, std::move(*decoded));
    }
    return params;
}

} // namespace

bool looksLikeDeepLink(std::string_view argument) noexcept {
    return argument.size() >= kSchemeShort.size() &&
           equalsIgnoreCase(argument.substr(0, kSchemeShort.size()), kSchemeShort);
}

core::Outcome<DeepLink> parseDeepLink(std::string_view url, std::span<const std::string> routeHosts) {
    if (url.empty()) {
        return fail(Error::invalidInput("deep link: empty"));
    }
    if (url.size() > kMaxDeepLinkLength) {
        return fail(Error::invalidInput("deep link: too long"));
    }
    for (const char c : url) {
        if (isRawForbidden(c)) {
            return fail(Error::invalidInput("deep link: forbidden character"));
        }
        if (c == '#') {
            return fail(Error::invalidInput("deep link: fragments are not supported"));
        }
    }
    if (url.size() < kSchemePrefix.size() || !equalsIgnoreCase(url.substr(0, kSchemePrefix.size()), kSchemePrefix)) {
        return fail(Error::invalidInput("deep link: wrong scheme"));
    }
    const std::string_view rest = url.substr(kSchemePrefix.size());
    const std::size_t questionMark = rest.find('?');
    const std::string_view pathPart = rest.substr(0, questionMark);
    const std::string_view query =
        questionMark == std::string_view::npos ? std::string_view{} : rest.substr(questionMark + 1);

    auto route = parseRoute(pathPart, routeHosts);
    if (!route) {
        return fail(route.error());
    }
    auto params = parseQuery(query);
    if (!params) {
        return fail(params.error());
    }
    return DeepLink{std::move(*route), std::move(*params)};
}

} // namespace aedst::platform
