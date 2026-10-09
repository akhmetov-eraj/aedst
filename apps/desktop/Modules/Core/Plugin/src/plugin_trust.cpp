#include "aedst/plugin/plugin_trust.hpp"

#include <algorithm>
#include <cctype>

namespace aedst::plugin {
namespace {

constexpr std::size_t kSha256HexChars = 64;

[[nodiscard]] bool isPlainFileName(std::string_view name) noexcept {
    return !name.empty() && name != "." && name != ".." && name.find('/') == std::string_view::npos &&
           name.find('\\') == std::string_view::npos && name.find(':') == std::string_view::npos &&
           name.find('\0') == std::string_view::npos;
}

[[nodiscard]] bool equalsHexIgnoreCase(std::string_view lhs, std::string_view rhs) noexcept {
    if (lhs.size() != kSha256HexChars || rhs.size() != kSha256HexChars) {
        return false;
    }
    return std::ranges::equal(lhs, rhs, [](char a, char b) {
        return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
    });
}

} // namespace

TrustDecision evaluateCandidate(const TrustPolicy& policy, const CandidateFile& candidate) {
    if (policy.allowlistPresent && !policy.allowlistSignatureValid) {
        return {.verdict = TrustVerdict::BadSignature, .entry = nullptr};
    }
    if (!policy.allowlistPresent) {
        if (!policy.releaseBuild && policy.allowUnsignedDevPlugins && isPlainFileName(candidate.fileName)) {
            return {.verdict = TrustVerdict::TrustedUnsignedDev, .entry = nullptr};
        }
        return {.verdict = TrustVerdict::NoAllowlist, .entry = nullptr};
    }
    if (!isPlainFileName(candidate.fileName)) {
        return {.verdict = TrustVerdict::NotListed, .entry = nullptr};
    }
    const auto it = std::ranges::find(policy.entries, candidate.fileName, &AllowlistEntry::fileName);
    if (it == policy.entries.end()) {
        return {.verdict = TrustVerdict::NotListed, .entry = nullptr};
    }
    if (!equalsHexIgnoreCase(it->sha256, candidate.sha256)) {
        return {.verdict = TrustVerdict::HashMismatch, .entry = nullptr};
    }
    if (it->size != candidate.size) {
        return {.verdict = TrustVerdict::SizeMismatch, .entry = nullptr};
    }
    return {.verdict = TrustVerdict::Trusted, .entry = &*it};
}

} // namespace aedst::plugin
