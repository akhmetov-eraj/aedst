#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace aedst::plugin {

/// One line of the signed allowlist that ships with the application (plugins.allowlist.json, signed
/// with the release key; see ADR-0033). A runtime plugin is loadable only if its file is listed here
/// with exactly this hash and size.
struct AllowlistEntry {
    /// File name only, no directory part.
    std::string fileName;
    /// 64 hex characters, SHA-256 of the whole file.
    std::string sha256;
    std::uint64_t size{0};
    /// The plugin id the file must declare in its metadata and descriptor.
    std::string pluginId;
};

struct TrustPolicy {
    /// True in every build with NDEBUG. A release build never loads an unlisted or unsigned file.
    bool releaseBuild{true};
    /// Development convenience for Debug builds only. IGNORED when releaseBuild is true.
    bool allowUnsignedDevPlugins{false};
    /// An allowlist file exists next to the plugins.
    bool allowlistPresent{false};
    /// Its detached Ed25519 signature verified against a trusted release key.
    bool allowlistSignatureValid{false};
    std::vector<AllowlistEntry> entries;
};

struct CandidateFile {
    std::string fileName;
    std::string sha256;
    std::uint64_t size{0};
};

enum class TrustVerdict {
    /// Listed in a validly signed allowlist with matching hash and size.
    Trusted,
    /// Debug builds only: no allowlist, unsigned development plugin explicitly allowed.
    TrustedUnsignedDev,
    NoAllowlist,
    BadSignature,
    NotListed,
    HashMismatch,
    SizeMismatch,
};

[[nodiscard]] constexpr std::string_view toString(TrustVerdict verdict) noexcept {
    switch (verdict) {
    case TrustVerdict::Trusted:
        return "trusted";
    case TrustVerdict::TrustedUnsignedDev:
        return "trusted-unsigned-dev";
    case TrustVerdict::NoAllowlist:
        return "no-allowlist";
    case TrustVerdict::BadSignature:
        return "bad-signature";
    case TrustVerdict::NotListed:
        return "not-listed";
    case TrustVerdict::HashMismatch:
        return "hash-mismatch";
    case TrustVerdict::SizeMismatch:
        return "size-mismatch";
    }
    return "unknown";
}

struct TrustDecision {
    TrustVerdict verdict{TrustVerdict::NoAllowlist};
    /// The matching entry for Trusted, otherwise nullptr. Points into the policy passed to
    /// evaluateCandidate, which must outlive the decision.
    const AllowlistEntry* entry{nullptr};

    [[nodiscard]] bool loadable() const noexcept {
        return verdict == TrustVerdict::Trusted || verdict == TrustVerdict::TrustedUnsignedDev;
    }
};

/// Pure decision, no I/O. Fails closed:
///  - an allowlist that is present but not validly signed rejects everything, in every build;
///  - without an allowlist only a non-release build with allowUnsignedDevPlugins loads anything;
///  - with a valid allowlist the file must be listed by exact name (a name with a path separator or
///    ".." is never listed) with equal SHA-256 (hex compared case-insensitively) and size.
[[nodiscard]] TrustDecision evaluateCandidate(const TrustPolicy& policy, const CandidateFile& candidate);

} // namespace aedst::plugin
