#pragma once

#include "aedst/core/outcome.hpp"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace aedst::persistence {

/// Encrypts small secrets (session tokens) before they touch disk.
///
/// - Windows: DPAPI (CryptProtectData). The key is derived by the OS from the
///   user's logon credentials, so nothing key-shaped is ever stored by us and
///   another Windows account on the same machine can't decrypt the blob.
/// - Everywhere else (Linux Flatpak/AppImage): AES-256-GCM via OpenSSL with a
///   random per-install key file (mode 0600) next to the database. This is
///   weaker than a desktop keyring because the key lives beside the data, but it
///   has no runtime dependency on a Secret Service daemon being reachable from
///   inside a Flatpak sandbox. Scope cut: libsecret/KWallet integration.
class SecureStorage final {
public:
    /// `keyFile` is only used by the non-Windows (OpenSSL) implementation.
    explicit SecureStorage(std::filesystem::path keyFile);

    [[nodiscard]] core::Outcome<std::vector<std::uint8_t>> protect(std::span<const std::uint8_t> plaintext) const;
    [[nodiscard]] core::Outcome<std::vector<std::uint8_t>> unprotect(std::span<const std::uint8_t> ciphertext) const;

    /// Hex SHA-256, for correlating secrets in logs/diagnostics without storing
    /// or printing the secret itself.
    [[nodiscard]] static core::Outcome<std::string> sha256Hex(std::string_view input);

private:
#if !defined(_WIN32)
    [[nodiscard]] core::Outcome<std::vector<std::uint8_t>> loadOrCreateKey() const;
#endif

    std::filesystem::path keyFile_;
};

} // namespace aedst::persistence
