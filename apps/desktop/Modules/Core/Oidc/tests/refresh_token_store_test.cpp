#include "aedst/oidc/refresh_token_store.hpp"
#include "aedst/testing/fake_http_server.hpp"

#include <gtest/gtest.h>

#include <optional>

namespace aedst::oidc {
namespace {

using aedst::testing::waitUntil;

StoredIdentity identity() {
    return StoredIdentity{.refreshToken = core::SecretString("rt-secret"), .subject = "user-1"};
}

TEST(RefreshTokenStoreTest, SerializationRoundTrips) {
    const std::string text = serializeIdentity(identity());
    const auto parsed = parseIdentity(text);
    ASSERT_TRUE(parsed.has_value()) << parsed.error().message;
    EXPECT_EQ(parsed->refreshToken.reveal(), "rt-secret");
    EXPECT_EQ(parsed->subject, "user-1");
}

TEST(RefreshTokenStoreTest, ParseRejectsCorruptUnknownVersionAndIncompleteData) {
    EXPECT_FALSE(parseIdentity("not json").has_value());
    EXPECT_FALSE(parseIdentity(R"({"v":2,"rt":"x","sub":"y"})").has_value());
    EXPECT_FALSE(parseIdentity(R"({"v":1,"rt":"","sub":"y"})").has_value());
    EXPECT_FALSE(parseIdentity(R"({"v":1,"rt":"x"})").has_value());
    EXPECT_FALSE(parseIdentity(R"([])").has_value());
}

TEST(RefreshTokenStoreTest, EntryNameIsStablePerIssuerAndClientAndHidesTheUrl) {
    const std::string a = keychainEntryName("https://id.example.com", "client-1");
    EXPECT_EQ(a, keychainEntryName("https://id.example.com", "client-1"));
    EXPECT_NE(a, keychainEntryName("https://staging.example.com", "client-1"));
    EXPECT_NE(a, keychainEntryName("https://id.example.com", "client-2"));
    EXPECT_EQ(a.rfind("oidc-", 0), 0U);
    EXPECT_EQ(a.find("example"), std::string::npos);
    EXPECT_EQ(a.size(), 5U + 24U);
}

TEST(InMemoryRefreshTokenStoreTest, SaveLoadClear) {
    InMemoryRefreshTokenStore store;
    std::optional<core::Outcome<std::optional<StoredIdentity>>> loaded;
    store.load([&](core::Outcome<std::optional<StoredIdentity>> r) { loaded = std::move(r); });
    ASSERT_TRUE(waitUntil([&] { return loaded.has_value(); }));
    ASSERT_TRUE(loaded->has_value());
    EXPECT_FALSE((*loaded)->has_value()); // nothing stored is not an error

    bool saved = false;
    store.save(identity(), [&](core::VoidOutcome r) { saved = r.has_value(); });
    ASSERT_TRUE(waitUntil([&] { return saved; }));
    EXPECT_TRUE(store.hasIdentity());

    loaded.reset();
    store.load([&](core::Outcome<std::optional<StoredIdentity>> r) { loaded = std::move(r); });
    ASSERT_TRUE(waitUntil([&] { return loaded.has_value(); }));
    ASSERT_TRUE((*loaded)->has_value());
    EXPECT_EQ((**loaded)->subject, "user-1");

    bool cleared = false;
    store.clear([&](core::VoidOutcome r) { cleared = r.has_value(); });
    ASSERT_TRUE(waitUntil([&] { return cleared; }));
    EXPECT_FALSE(store.hasIdentity());
}

TEST(InMemoryRefreshTokenStoreTest, UnavailableStoreReportsAStorageErrorAndNeverFallsBack) {
    InMemoryRefreshTokenStore store;
    store.setUnavailable(true);
    std::optional<core::VoidOutcome> saved;
    store.save(identity(), [&](core::VoidOutcome r) { saved = std::move(r); });
    ASSERT_TRUE(waitUntil([&] { return saved.has_value(); }));
    ASSERT_FALSE(saved->has_value());
    EXPECT_EQ(saved->error().code, core::ErrorCode::Storage);
    EXPECT_FALSE(store.hasIdentity());
}

} // namespace
} // namespace aedst::oidc
