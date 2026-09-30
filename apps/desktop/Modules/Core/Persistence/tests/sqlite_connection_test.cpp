#include "aedst/persistence/sqlite_connection.hpp"

#include <gtest/gtest.h>

#include <cstdint>
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

} // namespace
} // namespace aedst::persistence
