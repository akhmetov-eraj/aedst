#include "aedst/persistence/secure_storage.hpp"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dpapi.h>
#else
#include <fstream>
#include <iterator>
#endif

namespace aedst::persistence {
namespace {

constexpr std::array<char, 16> kHexDigits{'0', '1', '2', '3', '4', '5', '6', '7',
                                          '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

} // namespace

SecureStorage::SecureStorage(std::filesystem::path keyFile) : keyFile_(std::move(keyFile)) {}

core::Outcome<std::string> SecureStorage::sha256Hex(std::string_view input) {
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int length = 0;
    if (EVP_Digest(input.data(), input.size(), digest.data(), &length, EVP_sha256(), nullptr) != 1) {
        return core::fail(core::Error::storage("EVP_Digest(sha256) failed"));
    }
    std::string hex;
    hex.reserve(static_cast<std::size_t>(length) * 2U);
    for (unsigned int i = 0; i < length; ++i) {
        const unsigned char byte = digest.at(i);
        hex.push_back(kHexDigits.at(byte >> 4U));
        hex.push_back(kHexDigits.at(byte & 0x0FU));
    }
    return hex;
}

#if defined(_WIN32)

// --- Windows: DPAPI --------------------------------------------------------------

core::Outcome<std::vector<std::uint8_t>> SecureStorage::protect(std::span<const std::uint8_t> plaintext) const {
    DATA_BLOB input{static_cast<DWORD>(plaintext.size()), const_cast<BYTE*>(plaintext.data())};
    DATA_BLOB output{};
    if (CryptProtectData(&input, L"aedst-secret", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output) ==
        FALSE) {
        return core::fail(core::Error::storage("CryptProtectData failed: " + std::to_string(GetLastError())));
    }
    std::vector<std::uint8_t> result(output.pbData, output.pbData + output.cbData);
    LocalFree(output.pbData);
    return result;
}

core::Outcome<std::vector<std::uint8_t>> SecureStorage::unprotect(std::span<const std::uint8_t> ciphertext) const {
    DATA_BLOB input{static_cast<DWORD>(ciphertext.size()), const_cast<BYTE*>(ciphertext.data())};
    DATA_BLOB output{};
    if (CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output) ==
        FALSE) {
        return core::fail(core::Error::storage("CryptUnprotectData failed: " + std::to_string(GetLastError())));
    }
    std::vector<std::uint8_t> result(output.pbData, output.pbData + output.cbData);
    SecureZeroMemory(output.pbData, output.cbData);
    LocalFree(output.pbData);
    return result;
}

#else

// --- Other platforms: OpenSSL AES-256-GCM -----------------------------------------
// Blob layout: [12-byte IV][ciphertext][16-byte GCM tag]

namespace {

constexpr std::size_t kKeySize = 32;
constexpr std::size_t kIvSize = 12;
constexpr std::size_t kTagSize = 16;

using CipherContext = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;

CipherContext makeContext() {
    return {EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free};
}

} // namespace

core::Outcome<std::vector<std::uint8_t>> SecureStorage::loadOrCreateKey() const {
    std::error_code ec;
    if (std::filesystem::exists(keyFile_, ec)) {
        std::ifstream in(keyFile_, std::ios::binary);
        std::vector<std::uint8_t> key((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (key.size() != kKeySize) {
            return core::fail(core::Error::storage("secure storage key file is corrupt"));
        }
        return key;
    }

    std::vector<std::uint8_t> key(kKeySize);
    if (RAND_bytes(key.data(), static_cast<int>(key.size())) != 1) {
        return core::fail(core::Error::storage("RAND_bytes failed"));
    }
    if (keyFile_.has_parent_path()) {
        std::filesystem::create_directories(keyFile_.parent_path(), ec);
    }
    {
        std::ofstream out(keyFile_, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(key.data()), static_cast<std::streamsize>(key.size()));
        if (!out) {
            return core::fail(core::Error::storage("failed to write secure storage key file"));
        }
    }
    std::filesystem::permissions(keyFile_, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                                 std::filesystem::perm_options::replace, ec);
    return key;
}

core::Outcome<std::vector<std::uint8_t>> SecureStorage::protect(std::span<const std::uint8_t> plaintext) const {
    auto key = loadOrCreateKey();
    if (!key) {
        return core::fail(key.error());
    }

    std::vector<std::uint8_t> blob(kIvSize + plaintext.size() + kTagSize);
    if (RAND_bytes(blob.data(), static_cast<int>(kIvSize)) != 1) {
        return core::fail(core::Error::storage("RAND_bytes failed"));
    }

    const CipherContext ctx = makeContext();
    int length = 0;
    int finalLength = 0;
    std::uint8_t* const out = blob.data() + kIvSize;
    if (!ctx || EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, key->data(), blob.data()) != 1 ||
        EVP_EncryptUpdate(ctx.get(), out, &length, plaintext.data(), static_cast<int>(plaintext.size())) != 1 ||
        EVP_EncryptFinal_ex(ctx.get(), out + length, &finalLength) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, static_cast<int>(kTagSize),
                            blob.data() + kIvSize + plaintext.size()) != 1) {
        return core::fail(core::Error::storage("AES-256-GCM encryption failed"));
    }
    return blob;
}

core::Outcome<std::vector<std::uint8_t>> SecureStorage::unprotect(std::span<const std::uint8_t> ciphertext) const {
    if (ciphertext.size() < kIvSize + kTagSize) {
        return core::fail(core::Error::storage("ciphertext too short"));
    }
    auto key = loadOrCreateKey();
    if (!key) {
        return core::fail(key.error());
    }

    const auto iv = ciphertext.first(kIvSize);
    const auto body = ciphertext.subspan(kIvSize, ciphertext.size() - kIvSize - kTagSize);
    std::array<std::uint8_t, kTagSize> tag{};
    const auto tagSpan = ciphertext.last(kTagSize);
    std::copy(tagSpan.begin(), tagSpan.end(), tag.begin());

    std::vector<std::uint8_t> plaintext(body.size());
    const CipherContext ctx = makeContext();
    int length = 0;
    int finalLength = 0;
    if (!ctx || EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, key->data(), iv.data()) != 1 ||
        EVP_DecryptUpdate(ctx.get(), plaintext.data(), &length, body.data(), static_cast<int>(body.size())) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, static_cast<int>(kTagSize), tag.data()) != 1) {
        return core::fail(core::Error::storage("AES-256-GCM decryption setup failed"));
    }
    // Final fails when the tag doesn't verify: tampered data or a different key.
    if (EVP_DecryptFinal_ex(ctx.get(), plaintext.data() + length, &finalLength) != 1) {
        return core::fail(core::Error::storage("AES-256-GCM authentication failed"));
    }
    plaintext.resize(static_cast<std::size_t>(length + finalLength));
    return plaintext;
}

#endif

} // namespace aedst::persistence
