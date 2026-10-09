#include "aedst/update/semver.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace aedst::update {
namespace {

using core::Error;
using core::fail;

constexpr std::size_t kMaxVersionLength = 64;
constexpr std::size_t kMaxCoreDigits = 18;

bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

bool isIdentifierChar(char c) {
    return isDigit(c) || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '-';
}

bool allDigits(std::string_view text) {
    return !text.empty() && std::all_of(text.begin(), text.end(), isDigit);
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

std::optional<std::uint64_t> parseCoreNumber(std::string_view text) {
    if (!allDigits(text) || text.size() > kMaxCoreDigits || (text.size() > 1 && text.front() == '0')) {
        return std::nullopt;
    }
    std::uint64_t value = 0;
    for (const char c : text) {
        value = value * 10U + static_cast<std::uint64_t>(c - '0');
    }
    return value;
}

bool isValidIdentifier(std::string_view id) {
    return !id.empty() && std::all_of(id.begin(), id.end(), isIdentifierChar);
}

std::strong_ordering compareNumericStrings(std::string_view a, std::string_view b) {
    // No leading zeros are allowed, so a longer digit string is a larger number.
    if (a.size() != b.size()) {
        return a.size() <=> b.size();
    }
    return a.compare(b) <=> 0;
}

std::strong_ordering compareIdentifiers(const std::string& a, const std::string& b) {
    const bool aNumeric = allDigits(a);
    const bool bNumeric = allDigits(b);
    if (aNumeric && bNumeric) {
        return compareNumericStrings(a, b);
    }
    if (aNumeric) {
        return std::strong_ordering::less;
    }
    if (bNumeric) {
        return std::strong_ordering::greater;
    }
    return a.compare(b) <=> 0;
}

} // namespace

std::string SemVer::toString() const {
    std::string out = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    for (std::size_t i = 0; i < prerelease.size(); ++i) {
        out.push_back(i == 0 ? '-' : '.');
        out += prerelease[i];
    }
    if (!buildMetadata.empty()) {
        out.push_back('+');
        out += buildMetadata;
    }
    return out;
}

core::Outcome<SemVer> parseSemVer(std::string_view text) {
    if (text.empty() || text.size() > kMaxVersionLength) {
        return fail(Error::invalidInput("semver: empty or too long"));
    }
    SemVer version;
    std::string_view rest = text;

    if (const std::size_t plus = rest.find('+'); plus != std::string_view::npos) {
        const std::string_view build = rest.substr(plus + 1);
        for (const std::string_view id : split(build, '.')) {
            if (!isValidIdentifier(id)) {
                return fail(Error::invalidInput("semver: invalid build metadata"));
            }
        }
        version.buildMetadata = std::string{build};
        rest = rest.substr(0, plus);
    }
    if (const std::size_t dash = rest.find('-'); dash != std::string_view::npos) {
        const std::string_view pre = rest.substr(dash + 1);
        for (const std::string_view id : split(pre, '.')) {
            if (!isValidIdentifier(id)) {
                return fail(Error::invalidInput("semver: invalid pre-release"));
            }
            if (allDigits(id) && id.size() > 1 && id.front() == '0') {
                return fail(Error::invalidInput("semver: leading zero in numeric pre-release identifier"));
            }
            version.prerelease.emplace_back(id);
        }
        rest = rest.substr(0, dash);
    }
    const std::vector<std::string_view> numbers = split(rest, '.');
    if (numbers.size() != 3) {
        return fail(Error::invalidInput("semver: expected MAJOR.MINOR.PATCH"));
    }
    const auto major = parseCoreNumber(numbers[0]);
    const auto minor = parseCoreNumber(numbers[1]);
    const auto patch = parseCoreNumber(numbers[2]);
    if (!major || !minor || !patch) {
        return fail(Error::invalidInput("semver: invalid numeric component"));
    }
    version.major = *major;
    version.minor = *minor;
    version.patch = *patch;
    return version;
}

std::strong_ordering compareSemVer(const SemVer& lhs, const SemVer& rhs) {
    if (const auto c = lhs.major <=> rhs.major; c != 0) {
        return c;
    }
    if (const auto c = lhs.minor <=> rhs.minor; c != 0) {
        return c;
    }
    if (const auto c = lhs.patch <=> rhs.patch; c != 0) {
        return c;
    }
    if (lhs.prerelease.empty() && rhs.prerelease.empty()) {
        return std::strong_ordering::equal;
    }
    if (lhs.prerelease.empty()) {
        return std::strong_ordering::greater;
    }
    if (rhs.prerelease.empty()) {
        return std::strong_ordering::less;
    }
    const std::size_t shared = std::min(lhs.prerelease.size(), rhs.prerelease.size());
    for (std::size_t i = 0; i < shared; ++i) {
        if (const auto c = compareIdentifiers(lhs.prerelease[i], rhs.prerelease[i]); c != 0) {
            return c;
        }
    }
    return lhs.prerelease.size() <=> rhs.prerelease.size();
}

} // namespace aedst::update
