#include "aedst/persistence/master_key_provider.hpp"
#include "temp_directory.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace aedst::persistence {
namespace {

/// Keyring double sharing its state with the test through a pointer.
struct FakeKeyringState {
    std::optional<std::vector<std::uint8_t>> entry;
    bool unavailable{false};
    int storeCalls{0};
};

class FakeKeyringBackend final : public KeyringBackend {
public:
    explicit FakeKeyringBackend(FakeKeyringState* state) : state_(state) {}

    [[nodiscard]] std::string_view name() const noexcept override { return "fake"; }

    [[nodiscard]] core::Outcome<std::optional<SecretBytes>> lookup() override {
        if (state_->unavailable) {
            return core::fail(core::Error::storage("keyring unavailable"));
        }
        if (!state_->entry) {
            return std::optional<SecretBytes>{};
        }
        return std::optional<SecretBytes>{SecretBytes{*state_->entry}};
    }

    [[nodiscard]] core::VoidOutcome store(std::span<const std::uint8_t> key) override {
        if (state_->unavailable) {
            return core::fail(core::Error::storage("keyring unavailable"));
        }
        ++state_->storeCalls;
        state_->entry = std::vector<std::uint8_t>(key.begin(), key.end());
        return core::ok();
    }

private:
    FakeKeyringState* state_;
};

std::vector<std::uint8_t> toVector(const SecretBytes& key) {
    return {key.view().begin(), key.view().end()};
}

TEST(FileMasterKeyProviderTest, CreatesKeyOnceAndReturnsSameKeyAfterwards) {
    const test_support::TempDirectory dir;
    const auto keyFile = dir.path() / "nested" / "secure.key";
    FileMasterKeyProvider provider(keyFile);

    auto first = provider.loadOrCreate();
    ASSERT_TRUE(first.has_value()) << first.error().message;
    EXPECT_EQ(first->size(), kMasterKeySize);

    FileMasterKeyProvider again(keyFile);
    auto second = again.loadOrCreate();
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(toVector(*first), toVector(*second));
}

TEST(FileMasterKeyProviderTest, LeavesNoTemporaryFilesBehind) {
    const test_support::TempDirectory dir;
    FileMasterKeyProvider provider(dir.path() / "secure.key");
    ASSERT_TRUE(provider.loadOrCreate().has_value());

    std::size_t entries = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir.path())) {
        static_cast<void>(entry);
        ++entries;
    }
    EXPECT_EQ(entries, 1U);
}

TEST(KeyringMasterKeyProviderTest, GeneratesAndStoresKeyWhenKeyringIsEmpty) {
    FakeKeyringState state;
    KeyringMasterKeyProvider provider(std::make_unique<FakeKeyringBackend>(&state), {});

    auto key = provider.loadOrCreate();
    ASSERT_TRUE(key.has_value()) << key.error().message;

    ASSERT_TRUE(state.entry.has_value());
    EXPECT_EQ(*state.entry, toVector(*key));
    EXPECT_EQ(state.storeCalls, 1);
}

TEST(KeyringMasterKeyProviderTest, ReusesExistingKeyringEntry) {
    FakeKeyringState state;
    state.entry = std::vector<std::uint8_t>(kMasterKeySize, 0x07);
    KeyringMasterKeyProvider provider(std::make_unique<FakeKeyringBackend>(&state), {});

    auto key = provider.loadOrCreate();

    ASSERT_TRUE(key.has_value());
    EXPECT_EQ(toVector(*key), *state.entry);
    EXPECT_EQ(state.storeCalls, 0);
}

TEST(KeyringMasterKeyProviderTest, MigratesLegacyKeyFileAndDeletesIt) {
    const test_support::TempDirectory dir;
    const auto legacy = dir.path() / "secure.key";
    const std::vector<std::uint8_t> legacyKey(kMasterKeySize, 0x5A);
    {
        std::ofstream out(legacy, std::ios::binary);
        out.write(reinterpret_cast<const char*>(legacyKey.data()), static_cast<std::streamsize>(legacyKey.size()));
    }
    FakeKeyringState state;
    KeyringMasterKeyProvider provider(std::make_unique<FakeKeyringBackend>(&state), legacy);

    auto key = provider.loadOrCreate();

    ASSERT_TRUE(key.has_value()) << key.error().message;
    EXPECT_EQ(toVector(*key), legacyKey);
    ASSERT_TRUE(state.entry.has_value());
    EXPECT_EQ(*state.entry, legacyKey);
    EXPECT_FALSE(std::filesystem::exists(legacy));
}

TEST(KeyringMasterKeyProviderTest, KeepsLegacyFileWhenKeyringStoreFails) {
    const test_support::TempDirectory dir;
    const auto legacy = dir.path() / "secure.key";
    {
        std::ofstream out(legacy, std::ios::binary);
        const std::string key(kMasterKeySize, 'k');
        out << key;
    }
    FakeKeyringState state;
    state.unavailable = true;
    KeyringMasterKeyProvider provider(std::make_unique<FakeKeyringBackend>(&state), legacy);

    EXPECT_FALSE(provider.loadOrCreate().has_value());
    EXPECT_TRUE(std::filesystem::exists(legacy));
}

TEST(KeyringMasterKeyProviderTest, RejectsCorruptKeyringEntryWithoutOverwriting) {
    FakeKeyringState state;
    state.entry = std::vector<std::uint8_t>(5, 0x01);
    KeyringMasterKeyProvider provider(std::make_unique<FakeKeyringBackend>(&state), {});

    EXPECT_FALSE(provider.loadOrCreate().has_value());
    EXPECT_EQ(state.storeCalls, 0);
}

TEST(SecretBytesTest, MoveEmptiesSource) {
    SecretBytes original(std::vector<std::uint8_t>(4, 0x09));
    const SecretBytes moved(std::move(original));

    EXPECT_EQ(moved.size(), 4U);
    EXPECT_TRUE(original.empty()); // NOLINT(bugprone-use-after-move): moved-from is specified empty
}

} // namespace
} // namespace aedst::persistence
