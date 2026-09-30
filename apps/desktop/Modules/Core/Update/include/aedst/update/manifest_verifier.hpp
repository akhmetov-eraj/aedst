#pragma once

#include "aedst/core/outcome.hpp"
#include "aedst/update/manifest.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace aedst::update {

inline constexpr std::size_t kEd25519PublicKeyBytes = 32;
inline constexpr std::size_t kEd25519SignatureBytes = 64;
inline constexpr std::size_t kMaxEnvelopeBytes = 1024 * 1024;

/// One trusted release-signing key. Several may be trusted at once: that is how
/// keys are rotated (ship a build that trusts old+new, sign with the new one,
/// later drop the old one).
struct TrustedKey {
    std::string keyId;
    std::array<std::uint8_t, kEd25519PublicKeyBytes> publicKey{};

    friend bool operator==(const TrustedKey&, const TrustedKey&) = default;
};

/// Parses "keyId:<64 hex>,keyId2:<64 hex>" (the format of the
/// AEDST_UPDATE_PUBLIC_KEYS CMake variable). Empty input is a valid, empty list.
/// keyId: 1..32 characters of [A-Za-z0-9._-]; duplicates are rejected.
[[nodiscard]] core::Outcome<std::vector<TrustedKey>> parseTrustedKeys(std::string_view spec);

/// The keys compiled into this binary via AEDST_UPDATE_PUBLIC_KEYS. Empty (and
/// therefore verification fails closed) when none were configured or the value
/// is malformed.
[[nodiscard]] std::vector<TrustedKey> embeddedTrustedKeys();

/// Raw Ed25519 verification through OpenSSL (EVP_PKEY_ED25519 +
/// EVP_DigestVerify). Returns false on any error, never throws.
[[nodiscard]] bool verifyEd25519(std::span<const std::uint8_t> publicKey, std::span<const std::uint8_t> message,
                                 std::span<const std::uint8_t> signature);

/// Verifies a signed manifest envelope and parses it.
///
/// Wire format (JSON):
///   { "payload": "<base64 of the manifest JSON bytes>",
///     "signatures": [ { "keyId": "k1", "signature": "<base64 of 64 bytes>" } ] }
///
/// The signed "canonical manifest bytes" are exactly the decoded `payload`
/// bytes. They are never re-serialised, so there is no canonicalisation
/// ambiguity between the signing tool and this verifier. The envelope is
/// accepted when at least one signature verifies under the trusted key with the
/// same keyId; unknown keyIds are ignored (this is what allows rotation).
/// Anything else (no trusted keys, bad base64, wrong signature length, bad
/// signature, invalid manifest) is an error. Fails closed.
class ManifestVerifier final {
public:
    explicit ManifestVerifier(std::vector<TrustedKey> trustedKeys);

    [[nodiscard]] core::Outcome<UpdateManifest> verify(std::string_view envelopeJson) const;

private:
    std::vector<TrustedKey> trustedKeys_;
};

} // namespace aedst::update
