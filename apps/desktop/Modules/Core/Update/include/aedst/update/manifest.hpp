#pragma once

#include "aedst/core/outcome.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace aedst::update {

inline constexpr std::uint64_t kMaxArtifactBytes = 4ULL * 1024ULL * 1024ULL * 1024ULL;
inline constexpr std::size_t kMaxManifestArtifacts = 32;

struct ArtifactInfo {
    /// "windows" | "linux" (lowercase [a-z0-9_], at most 16 characters).
    std::string platform;
    /// "x86_64" | "arm64" | ...
    std::string arch;
    /// https URL (isSecureHttpsUrl).
    std::string url;
    /// 64 lowercase hex characters (SHA-256 of the file).
    std::string sha256;
    std::uint64_t size{0};
    /// Optional detached-signature reference carried through untouched. It is
    /// NOT used for trust decisions: the manifest signature already covers url,
    /// sha256 and size.
    std::string signature;

    friend bool operator==(const ArtifactInfo&, const ArtifactInfo&) = default;
};

struct UpdateManifest {
    std::string version;
    std::string channel;
    std::string releaseNotesUrl;
    std::vector<ArtifactInfo> artifacts;
    /// Installed versions below this must update (mandatory update). Defaults to "0.0.0".
    std::string minSupportedVersion{"0.0.0"};
    /// Unix time in seconds (integer). Used for replay/rollback protection.
    std::int64_t publishedAt{0};

    friend bool operator==(const UpdateManifest&, const UpdateManifest&) = default;
};

/// Strict parse + validation of the manifest JSON (Qt's QJsonDocument does the
/// tokenising; Qt6::Core is the only Qt module this module uses). Required:
/// version (semver), channel ([a-z0-9-]{1,32}), releaseNotesUrl (https),
/// publishedAt (positive integer), artifacts (1..32 entries, each with platform,
/// arch, https url, 64-hex sha256, size in 1..kMaxArtifactBytes; no duplicate
/// platform/arch pair). Optional: minSupportedVersion (semver). Unknown fields
/// are ignored.
///
/// NOTE: this does not check any signature; use ManifestVerifier::verify().
[[nodiscard]] core::Outcome<UpdateManifest> parseManifest(std::string_view json);

/// The artifact for exactly this platform/arch, or nullptr.
[[nodiscard]] const ArtifactInfo* findArtifact(const UpdateManifest& manifest, std::string_view platform,
                                               std::string_view arch);

} // namespace aedst::update
