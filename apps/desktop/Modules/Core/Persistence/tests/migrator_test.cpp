#include "aedst/persistence/migrator.hpp"
#include "aedst/persistence/sqlite_connection.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace aedst::persistence {
namespace {

std::int64_t userVersion(SqliteConnection& connection) {
    auto statement = connection.prepare("PRAGMA user_version");
    EXPECT_TRUE(statement.has_value());
    EXPECT_TRUE(statement->step().value_or(false));
    return statement->columnInt64(0);
}

bool tableExists(SqliteConnection& connection, const std::string& name) {
    auto statement = connection.prepare("SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ?1");
    EXPECT_TRUE(statement.has_value());
    EXPECT_TRUE(statement->bind(1, std::string_view{name}));
    return statement->step().value_or(false);
}

class MigratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto connection = SqliteConnection::openInMemory();
        ASSERT_TRUE(connection.has_value());
        connection_ = std::make_unique<SqliteConnection>(std::move(*connection));
    }

    std::unique_ptr<SqliteConnection> connection_;
};

constexpr std::array<Migration, 2> kTwoSteps{{
    {.version = 1, .name = "create_a", .sql = "CREATE TABLE a (id INTEGER PRIMARY KEY);"},
    {.version = 2, .name = "create_b", .sql = "CREATE TABLE b (id INTEGER PRIMARY KEY); INSERT INTO b (id) VALUES (1);"},
}};

TEST_F(MigratorTest, FreshDatabaseReachesLatestVersion) {
    const Migrator migrator(kTwoSteps);

    auto version = migrator.migrate(*connection_);

    ASSERT_TRUE(version.has_value()) << version.error().message;
    EXPECT_EQ(*version, 2);
    EXPECT_EQ(userVersion(*connection_), 2);
    EXPECT_TRUE(tableExists(*connection_, "a"));
    EXPECT_TRUE(tableExists(*connection_, "b"));
}

TEST_F(MigratorTest, RerunIsANoOp) {
    const Migrator migrator(kTwoSteps);
    ASSERT_TRUE(migrator.migrate(*connection_).has_value());
    // A second run would fail with "table a already exists" if it re-applied step 1.
    auto again = migrator.migrate(*connection_);

    ASSERT_TRUE(again.has_value()) << again.error().message;
    EXPECT_EQ(*again, 2);
}

TEST_F(MigratorTest, AppliesOnlyMissingMigrationsToAnOlderDatabase) {
    const Migrator first(std::span<const Migration>(kTwoSteps).first(1));
    ASSERT_TRUE(first.migrate(*connection_).has_value());
    ASSERT_EQ(userVersion(*connection_), 1);

    const Migrator full(kTwoSteps);
    auto version = full.migrate(*connection_);

    ASSERT_TRUE(version.has_value()) << version.error().message;
    EXPECT_EQ(*version, 2);
    EXPECT_TRUE(tableExists(*connection_, "b"));
}

TEST_F(MigratorTest, RefusesDatabaseNewerThanKnown) {
    ASSERT_TRUE(connection_->execute("PRAGMA user_version = 99"));
    const Migrator migrator(kTwoSteps);

    auto version = migrator.migrate(*connection_);

    ASSERT_FALSE(version.has_value());
    EXPECT_EQ(version.error().code, core::ErrorCode::Storage);
    EXPECT_NE(version.error().message.find("newer"), std::string::npos);
    EXPECT_EQ(userVersion(*connection_), 99);
    EXPECT_FALSE(tableExists(*connection_, "a"));
}

TEST_F(MigratorTest, FailingMigrationRollsBackAndKeepsVersion) {
    static constexpr std::array<Migration, 2> kFailing{{
        {.version = 1, .name = "ok", .sql = "CREATE TABLE good (id INTEGER PRIMARY KEY);"},
        {.version = 2,
         .name = "broken",
         .sql = "CREATE TABLE half_done (id INTEGER PRIMARY KEY); THIS IS NOT SQL;"},
    }};
    const Migrator migrator(kFailing);

    auto version = migrator.migrate(*connection_);

    ASSERT_FALSE(version.has_value());
    EXPECT_NE(version.error().message.find("broken"), std::string::npos);
    EXPECT_EQ(userVersion(*connection_), 1); // step 1 committed, step 2 rolled back
    EXPECT_TRUE(tableExists(*connection_, "good"));
    EXPECT_FALSE(tableExists(*connection_, "half_done"));
}

TEST_F(MigratorTest, RejectsMigrationListWithGaps) {
    static constexpr std::array<Migration, 1> kGap{{
        {.version = 2, .name = "skips_one", .sql = "CREATE TABLE x (id INTEGER);"},
    }};
    const Migrator migrator(kGap);

    auto version = migrator.migrate(*connection_);

    ASSERT_FALSE(version.has_value());
    EXPECT_EQ(version.error().code, core::ErrorCode::InvalidArgument);
    EXPECT_FALSE(tableExists(*connection_, "x"));
}

TEST_F(MigratorTest, AppMigrationsCreateAuthSessionTable) {
    const Migrator migrator(appMigrations());

    auto version = migrator.migrate(*connection_);

    ASSERT_TRUE(version.has_value()) << version.error().message;
    EXPECT_EQ(*version, migrator.latestVersion());
    EXPECT_TRUE(tableExists(*connection_, "auth_session"));
}

TEST_F(MigratorTest, AppMigrationsAdoptPreMigratorDatabase) {
    // A database created by an earlier build: table present, user_version 0.
    ASSERT_TRUE(connection_->execute("CREATE TABLE auth_session (id INTEGER PRIMARY KEY CHECK (id = 1), "
                                     "user_id TEXT NOT NULL, access_token BLOB NOT NULL, refresh_token BLOB NOT NULL, "
                                     "access_expires_at INTEGER NOT NULL, refresh_fingerprint TEXT NOT NULL)"));
    const Migrator migrator(appMigrations());

    auto version = migrator.migrate(*connection_);

    ASSERT_TRUE(version.has_value()) << version.error().message;
    EXPECT_EQ(userVersion(*connection_), migrator.latestVersion());
}

TEST(TransactionTest, RollsBackUnlessCommitted) {
    auto opened = SqliteConnection::openInMemory();
    ASSERT_TRUE(opened.has_value());
    SqliteConnection connection = std::move(*opened);
    ASSERT_TRUE(connection.execute("CREATE TABLE t (id INTEGER PRIMARY KEY)"));

    {
        auto transaction = connection.beginTransaction();
        ASSERT_TRUE(transaction.has_value());
        ASSERT_TRUE(connection.execute("INSERT INTO t (id) VALUES (1)"));
    } // destroyed without commit
    {
        auto transaction = connection.beginTransaction();
        ASSERT_TRUE(transaction.has_value());
        ASSERT_TRUE(connection.execute("INSERT INTO t (id) VALUES (2)"));
        ASSERT_TRUE(transaction->commit());
    }

    auto count = connection.prepare("SELECT id FROM t");
    ASSERT_TRUE(count.has_value());
    ASSERT_TRUE(count->step().value_or(false));
    EXPECT_EQ(count->columnInt64(0), 2);
    EXPECT_FALSE(count->step().value_or(true));
}

} // namespace
} // namespace aedst::persistence
