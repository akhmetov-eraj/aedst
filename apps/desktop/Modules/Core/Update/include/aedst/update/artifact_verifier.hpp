#pragma once

#include "aedst/core/outcome.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

namespace aedst::update {

/// What the signed manifest promised about the downloaded file.
struct ExpectedArtifact {
    /// 64 hex characters (SHA-256).
    std::string sha256;
    /// Exact size in bytes.
    std::uint64_t size{0};
};

/// Constant-time equality (OpenSSL CRYPTO_memcmp); false when lengths differ.
[[nodiscard]] bool constantTimeEquals(std::span<const std::uint8_t> lhs, std::span<const std::uint8_t> rhs);

class ArtifactVerifier final {
public:
    ArtifactVerifier() = delete;

    /// Verifies a downloaded installer against the manifest before it may be
    /// used: the size must match exactly (checked up front and again while
    /// streaming, so a file that grows during hashing is caught), then the
    /// SHA-256 is computed in 64 KiB chunks (constant memory, however large the
    /// file) and compared in constant time.
    [[nodiscard]] static core::VoidOutcome verifyFile(const std::filesystem::path& file,
                                                      const ExpectedArtifact& expected);
};

} // namespace aedst::update
