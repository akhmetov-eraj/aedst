#pragma once

#include "aedst/core/outcome.hpp"
#include "aedst/persistence/master_key_provider.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace aedst::persistence {

/// Encrypts small secrets (session tokens) before they touch disk.
///
/// One algorithm on every platform: AES-256-GCM (OpenSSL EVP) under a 32-byte
/// master key obtained from a MasterKeyProvider (DPAPI-wrapped file on Windows,
/// Keychain on macOS, Secret Service on Linux, key file as fallback — see
/// createPlatformMasterKeyProvider()).
///
/// Blob layout: [1-byte format version][12-byte IV][ciphertext][16-byte GCM tag].
/// The additional authenticated data is the constant "aedst.secure.v1", so a
/// ciphertext from any other context does not authenticate. unprotect() rejects
/// unknown versions instead of guessing.
///
/// The master key is fetched once, cached in a wiped-on-destroy buffer, and
/// plaintext/key scratch buffers are wiped after use. Callers own (and should
/// wipe) the plaintext vector returned by unprotect().
///
/// Threading: protect()/unprotect() are safe from any thread (serialised by an
/// internal mutex; each call is microseconds).
class SecureStorage final {
public:
    static constexpr std::uint8_t kFormatVersion = 1;
    static constexpr std::size_t kMaxPlaintextSize = 1U << 20U;

    explicit SecureStorage(std::shared_ptr<MasterKeyProvider> keyProvider);

    [[nodiscard]] core::Outcome<std::vector<std::uint8_t>> protect(std::span<const std::uint8_t> plaintext) const;
    [[nodiscard]] core::Outcome<std::vector<std::uint8_t>> unprotect(std::span<const std::uint8_t> ciphertext) const;

    /// Hex SHA-256, for correlating secrets in logs/diagnostics without storing
    /// or printing the secret itself.
    [[nodiscard]] static core::Outcome<std::string> sha256Hex(std::string_view input);

private:
    /// Loads the key into key_ on first use. Caller must hold mutex_.
    [[nodiscard]] core::VoidOutcome ensureKeyLocked() const;

    std::shared_ptr<MasterKeyProvider> keyProvider_;
    mutable std::mutex mutex_;
    mutable std::optional<SecretBytes> key_;
};

} // namespace aedst::persistence
