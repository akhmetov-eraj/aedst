#include "aedst/update/url_validation.hpp"

#include <cstddef>
#include <string_view>

namespace aedst::update {
namespace {

constexpr std::size_t kMaxUrlLength = 2048;
constexpr std::size_t kMaxHostLength = 253;
constexpr std::size_t kMaxLabelLength = 63;
constexpr std::string_view kHttpsPrefix = "https://";

char asciiLower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return static_cast<char>(c - 'A' + 'a');
    }
    return c;
}

bool isHostLabelChar(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
}

bool isValidHost(std::string_view host) {
    if (host.empty() || host.size() > kMaxHostLength) {
        return false;
    }
    std::size_t labelStart = 0;
    while (labelStart <= host.size()) {
        std::size_t dot = host.find('.', labelStart);
        if (dot == std::string_view::npos) {
            dot = host.size();
        }
        const std::string_view label = host.substr(labelStart, dot - labelStart);
        if (label.empty() || label.size() > kMaxLabelLength || label.front() == '-' || label.back() == '-') {
            return false;
        }
        for (const char c : label) {
            if (!isHostLabelChar(c)) {
                return false;
            }
        }
        if (dot == host.size()) {
            return true;
        }
        labelStart = dot + 1;
    }
    return false;
}

bool isValidPort(std::string_view port) {
    if (port.empty() || port.size() > 5) {
        return false;
    }
    unsigned value = 0;
    for (const char c : port) {
        if (c < '0' || c > '9') {
            return false;
        }
        value = value * 10U + static_cast<unsigned>(c - '0');
    }
    return value >= 1U && value <= 65535U;
}

} // namespace

bool isSecureHttpsUrl(std::string_view url) {
    if (url.size() <= kHttpsPrefix.size() || url.size() > kMaxUrlLength) {
        return false;
    }
    for (std::size_t i = 0; i < kHttpsPrefix.size(); ++i) {
        if (asciiLower(url[i]) != kHttpsPrefix[i]) {
            return false;
        }
    }
    for (const char c : url) {
        const auto u = static_cast<unsigned char>(c);
        if (u <= 0x20U || u == 0x7FU || u >= 0x80U || c == '\\' || c == '"' || c == '<' || c == '>') {
            return false;
        }
    }
    const std::string_view rest = url.substr(kHttpsPrefix.size());
    const std::size_t authorityEnd = rest.find_first_of("/?#");
    const std::string_view authority = rest.substr(0, authorityEnd);
    if (authority.find('@') != std::string_view::npos) {
        return false;
    }
    const std::size_t colon = authority.rfind(':');
    if (colon == std::string_view::npos) {
        return isValidHost(authority);
    }
    return isValidHost(authority.substr(0, colon)) && isValidPort(authority.substr(colon + 1));
}

} // namespace aedst::update
