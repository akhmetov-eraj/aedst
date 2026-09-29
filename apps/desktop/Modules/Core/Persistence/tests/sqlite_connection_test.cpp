#include "aedst/persistence/secure_storage.hpp"
#include "aedst/persistence/sqlite_connection.hpp"
#include "aedst/persistence/token_store.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace aedst::persistence {
namespace {

TEST(SqliteConnectionTest, OpensInMemoryAndRoundTripsRows) {
    auto connection = SqliteConnection::openInMemory();
    ASSERT_TRUE(connection.has_value()) << connection.error().message;

    ASSERT_TRUE(connection->execute("CREATE TABLE notes (id INTEGER PRIMARY KEY, body TEXT NOT NULL)"));

    auto insert = connection->prepare("INSERT INTO notes (id, body) VALUES (?1, ?2)");
    ASSERT_TRUE(insert.has_value());
    ASSERT_TRUE(insert->bind(1, std::int64_t{7}));
    ASSERT_TRUE(insert->bind(2, std::string_view{"hello"}));
    auto inserted = insert->step();
    ASSERT_TRUE(inserted.has_value());
    EXPECT_FALSE(*inserted);

    auto select = connection->prepare("SELECT id, body FROM notes WHERE id = ?1");
    ASSERT_TRUE(select.has_value());
    ASSERT_TRUE(select->bind(1, std::int64_t{7}));
    auto hasRow = select->step();
    ASSERT_TRUE(hasRow.has_value());
    ASSERT_TRUE(*hasRow);
    EXPECT_EQ(select->columnInt64(0), 7);
    EXPECT_EQ(select->columnText(1), "hello");
}

TEST(SqliteConnectionTest, InvalidSqlReturnsStorageError) {
    auto connection = SqliteConnection::openInMemory();
    ASSERT_TRUE(connection.has_value());

    auto statement = connection->prepare("SELEC nonsense");

    ASSERT_FALSE(statement.has_value());
    EXPECT_EQ(statement.error().code, core::ErrorCode::Storage);
    EXPECT_FALSE(statement.error().message.empty());
}

TEST(SqliteConnectionTest, BlobsRoundTrip) {
    auto connection = SqliteConnection::openInMemory();
    ASSERT_TRUE(connection.has_value());
    ASSERT_TRUE(connection->execute("CREATE TABLE blobs (data BLOB)"));

    const std::vector<std::uint8_t> payload{0x00, 0xFF, 0x10, 0x20};
    auto insert = connection->prepare("INSERT INTO blobs (data) VALUES (?1)");
    ASSERT_TRUE(insert.has_value());
    ASSERT_TRUE(insert->bind(1, std::span<const std::uint8_t>(payload)));
    ASSERT_TRUE(insert->step().has_value());

    auto select = connection->prepare("SELECT data FROM blobs");
    ASSERT_TRUE(select.has_value());
    ASSERT_TRUE(select->step().value_or(false));
    EXPECT_EQ(select->columnBlob(0), payload);
}

class TokenStoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
        keyFile_ = std::filesystem::temp_directory_path() / ("aedst-test-" + std::to_string(unique)) / "secure.key";

        auto connection = SqliteConnection::openInMemory();
        ASSERT_TRUE(connection.has_value());
        connection_ = std::make_shared<SqliteConnection>(std::move(*connection));
        store_ = std::make_unique<TokenStore>(connection_, std::make_shared<SecureStorage>(keyFile_));
        ASSERT_TRUE(store_->migrate());
    }

    void TearDown() override {
        std::error_code ignored;
        std::filesystem::remove_all(keyFile_.parent_path(), ignored);
    }

    std::filesystem::path keyFile_;
    std::shared_ptr<SqliteConnection> connection_;
    std::unique_ptr<TokenStore> store_;
};

TEST_F(TokenStoreTest, LoadIsEmptyBeforeSave) {
    auto loaded = store_->load();

    ASSERT_TRUE(loaded.has_value());
    EXPECT_FALSE(loaded->has_value());
}

TEST_F(TokenStoreTest, SaveThenLoadReturnsSameTokens) {
    const StoredTokens tokens{
        .userId = "user-1",
        .accessToken = "access-token",
        .refreshToken = "refresh-token",
        .accessTokenExpiresAtEpochSeconds = 1'800'000'000,
    };

    ASSERT_TRUE(store_->save(tokens));
    auto loaded = store_->load();

    ASSERT_TRUE(loaded.has_value()) << loaded.error().message;
    ASSERT_TRUE(loaded->has_value());
    EXPECT_EQ(**loaded, tokens);
}

TEST_F(TokenStoreTest, TokensAreNotStoredInPlaintext) {
    ASSERT_TRUE(store_->save({.userId = "u", .accessToken = "plain-access", .refreshToken = "plain-refresh"}));

    auto select = connection_->prepare("SELECT access_token FROM auth_session");
    ASSERT_TRUE(select.has_value());
    ASSERT_TRUE(select->step().value_or(false));
    const auto blob = select->columnBlob(0);
    const std::string raw(blob.begin(), blob.end());
    EXPECT_EQ(raw.find("plain-access"), std::string::npos);
}

TEST_F(TokenStoreTest, ClearRemovesSession) {
    ASSERT_TRUE(store_->save({.userId = "u", .accessToken = "a", .refreshToken = "r"}));
    ASSERT_TRUE(store_->clear());

    auto loaded = store_->load();
    ASSERT_TRUE(loaded.has_value());
    EXPECT_FALSE(loaded->has_value());
}

TEST(SecureStorageTest, Sha256HexIsStable) {
    const auto digest = SecureStorage::sha256Hex("abc");

    ASSERT_TRUE(digest.has_value());
    EXPECT_EQ(*digest, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

} // namespace
} // namespace aedst::persistence
