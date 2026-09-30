#include "aedst/persistence/master_key_provider.hpp"
#include "aedst/persistence/secure_storage.hpp"
#include "in_memory_master_key_provider.hpp"
#include "temp_directory.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace aedst::persistence {
namespace {

std::vector<std::uint8_t> bytes(std::string_view text) {
    return {text.begin(), text.end()};
}

class SecureStorageTest : public ::testing::Test {
protected:
    [[nodiscard]] std::filesystem::path keyFile() const { return dir_.path() / "secure.key"; }

    [[nodiscard]] SecureStorage fileBacked() const {
        return SecureStorage(std::make_shared<FileMasterKeyProvider>(keyFile()));
    }

    test_support::TempDirectory dir_;
};

TEST_F(SecureStorageTest, RoundTripsWithFileKey) {
    const SecureStorage storage = fileBacked();
    const auto plaintext = bytes("super-secret-token");

    auto blob = storage.protect(plaintext);
    ASSERT_TRUE(blob.has_value()) << blob.error().message;
    EXPECT_EQ(blob->front(), SecureStorage::kFormatVersion);
    EXPECT_EQ(blob->size(), 1U + 12U + plaintext.size() + 16U);

    auto restored = storage.unprotect(*blob);
    ASSERT_TRUE(restored.has_value()) << restored.error().message;
    EXPECT_EQ(*restored, plaintext);
}

TEST_F(SecureStorageTest, KeyPersistsAcrossInstances) {
    auto blob = fileBacked().protect(bytes("persisted"));
    ASSERT_TRUE(blob.has_value());

    auto restored = fileBacked().unprotect(*blob);
    ASSERT_TRUE(restored.has_value()) << restored.error().message;
    EXPECT_EQ(*restored, bytes("persisted"));
}

TEST_F(SecureStorageTest, EmptyPlaintextRoundTrips) {
    const SecureStorage storage = fileBacked();

    auto blob = storage.protect(std::span<const std::uint8_t>{});
    ASSERT_TRUE(blob.has_value()) << blob.error().message;
    EXPECT_EQ(blob->size(), 1U + 12U + 16U);

    auto restored = storage.unprotect(*blob);
    ASSERT_TRUE(restored.has_value()) << restored.error().message;
    EXPECT_TRUE(restored->empty());
}

TEST_F(SecureStorageTest, CiphertextDoesNotContainPlaintext) {
    const SecureStorage storage = fileBacked();
    auto blob = storage.protect(bytes("recognisable-plaintext"));
    ASSERT_TRUE(blob.has_value());

    const std::string raw(blob->begin(), blob->end());
    EXPECT_EQ(raw.find("recognisable-plaintext"), std::string::npos);
}

TEST_F(SecureStorageTest, EncryptionIsRandomised) {
    const SecureStorage storage = fileBacked();
    auto first = storage.protect(bytes("same"));
    auto second = storage.protect(bytes("same"));
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_NE(*first, *second);
}

TEST_F(SecureStorageTest, TamperedCiphertextIsRejected) {
    const SecureStorage storage = fileBacked();
    auto blob = storage.protect(bytes("do-not-modify"));
    ASSERT_TRUE(blob.has_value());

    auto flippedBody = *blob;
    flippedBody[1U + 12U] = static_cast<std::uint8_t>(flippedBody[1U + 12U] ^ 0x01U); // first ciphertext byte
    EXPECT_FALSE(storage.unprotect(flippedBody).has_value());

    auto flippedTag = *blob;
    flippedTag.back() = static_cast<std::uint8_t>(flippedTag.back() ^ 0x80U);
    EXPECT_FALSE(storage.unprotect(flippedTag).has_value());

    auto flippedIv = *blob;
    flippedIv[1] = static_cast<std::uint8_t>(flippedIv[1] ^ 0x01U);
    EXPECT_FALSE(storage.unprotect(flippedIv).has_value());
}

TEST_F(SecureStorageTest, TruncatedBlobIsRejected) {
    const SecureStorage storage = fileBacked();
    auto blob = storage.protect(bytes("truncate-me"));
    ASSERT_TRUE(blob.has_value());

    auto oneByteShort = *blob;
    oneByteShort.pop_back();
    EXPECT_FALSE(storage.unprotect(oneByteShort).has_value());

    const std::vector<std::uint8_t> tiny(10, 0x01);
    EXPECT_FALSE(storage.unprotect(tiny).has_value());
    EXPECT_FALSE(storage.unprotect(std::span<const std::uint8_t>{}).has_value());
}

TEST_F(SecureStorageTest, UnknownVersionIsRejected) {
    const SecureStorage storage = fileBacked();
    auto blob = storage.protect(bytes("versioned"));
    ASSERT_TRUE(blob.has_value());

    auto future = *blob;
    future[0] = static_cast<std::uint8_t>(SecureStorage::kFormatVersion + 1U);
    auto result = storage.unprotect(future);

    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().message.find("version"), std::string::npos);
}

TEST_F(SecureStorageTest, WrongKeyIsRejected) {
    const SecureStorage writer(std::make_shared<test_support::InMemoryMasterKeyProvider>(0x01));
    const SecureStorage reader(std::make_shared<test_support::InMemoryMasterKeyProvider>(0x02));

    auto blob = writer.protect(bytes("for-writer-only"));
    ASSERT_TRUE(blob.has_value());

    EXPECT_FALSE(reader.unprotect(*blob).has_value());
    EXPECT_TRUE(writer.unprotect(*blob).has_value());
}

TEST_F(SecureStorageTest, MasterKeyIsLoadedOnlyOnce) {
    auto provider = std::make_shared<test_support::InMemoryMasterKeyProvider>();
    const SecureStorage storage(provider);

    for (int i = 0; i < 5; ++i) {
        auto blob = storage.protect(bytes("cache"));
        ASSERT_TRUE(blob.has_value());
        ASSERT_TRUE(storage.unprotect(*blob).has_value());
    }

    EXPECT_EQ(provider->loadCount, 1);
}

TEST_F(SecureStorageTest, FileKeyIsCreatedOwnerOnly) {
#if defined(_WIN32)
    GTEST_SKIP() << "POSIX file modes do not apply on Windows";
#else
    const SecureStorage storage = fileBacked();
    ASSERT_TRUE(storage.protect(bytes("x")).has_value());

    ASSERT_TRUE(std::filesystem::exists(keyFile()));
    EXPECT_EQ(std::filesystem::file_size(keyFile()), kMasterKeySize);
    const auto permissions = std::filesystem::status(keyFile()).permissions();
    EXPECT_EQ(permissions & std::filesystem::perms::all,
              std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
#endif
}

TEST_F(SecureStorageTest, CorruptKeyFileIsRejectedAndNotOverwritten) {
    {
        std::ofstream out(keyFile(), std::ios::binary);
        out << "too-short";
    }
    const SecureStorage storage = fileBacked();

    EXPECT_FALSE(storage.protect(bytes("x")).has_value());
    EXPECT_EQ(test_support::readAllBytes(keyFile()), "too-short");
}

TEST(Sha256Test, HexIsStable) {
    const auto digest = SecureStorage::sha256Hex("abc");

    ASSERT_TRUE(digest.has_value());
    EXPECT_EQ(*digest, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

} // namespace
} // namespace aedst::persistence
