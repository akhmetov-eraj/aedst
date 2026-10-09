#include "aedst/outbox/outbox_store.hpp"
#include "aedst/persistence/migrator.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <utility>

namespace aedst::outbox {
namespace {

using persistence::Migrator;
using persistence::SqliteConnection;

class OutboxStoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto opened = SqliteConnection::openInMemory();
        ASSERT_TRUE(opened.has_value());
        connection_ = std::make_unique<SqliteConnection>(std::move(*opened));
        ASSERT_TRUE(Migrator(persistence::appMigrations()).migrate(*connection_).has_value());
    }

    static OutboxItem item(const char* id, const char* type = "task.create", const char* payload = "{}") {
        return OutboxItem{.id = id, .type = type, .payload = payload, .createdAtMs = 42};
    }

    std::unique_ptr<SqliteConnection> connection_;
};

TEST_F(OutboxStoreTest, PendingItemsComeBackInCreationOrder) {
    ASSERT_TRUE(store::enqueue(*connection_, item("b")));
    ASSERT_TRUE(store::enqueue(*connection_, item("a")));
    ASSERT_TRUE(store::enqueue(*connection_, item("c", "task.update", R"({"id":"1"})")));

    const auto rows = store::pending(*connection_, 10);

    ASSERT_TRUE(rows.has_value());
    ASSERT_EQ(rows->size(), 3U);
    EXPECT_EQ((*rows)[0].id, "b");
    EXPECT_EQ((*rows)[1].id, "a");
    EXPECT_EQ((*rows)[2].id, "c");
    EXPECT_EQ((*rows)[2].type, "task.update");
    EXPECT_EQ((*rows)[2].payload, R"({"id":"1"})");
    EXPECT_EQ((*rows)[2].createdAtMs, 42);
    EXPECT_EQ((*rows)[2].attempts, 0);
    EXPECT_EQ((*rows)[2].status, OutboxStatus::Pending);
}

TEST_F(OutboxStoreTest, TheLimitCapsTheBatch) {
    for (const char* id : {"a", "b", "c"}) {
        ASSERT_TRUE(store::enqueue(*connection_, item(id)));
    }

    const auto rows = store::pending(*connection_, 2);

    ASSERT_TRUE(rows.has_value());
    EXPECT_EQ(rows->size(), 2U);
}

TEST_F(OutboxStoreTest, ADuplicateIdIsIgnoredAndKeepsTheFirstRow) {
    ASSERT_TRUE(store::enqueue(*connection_, item("a", "task.create", "first")));
    ASSERT_TRUE(store::enqueue(*connection_, item("a", "task.update", "second")));

    const auto rows = store::all(*connection_);

    ASSERT_TRUE(rows.has_value());
    ASSERT_EQ(rows->size(), 1U);
    EXPECT_EQ((*rows)[0].payload, "first");
    EXPECT_EQ((*rows)[0].type, "task.create");
}

TEST_F(OutboxStoreTest, RecordAttemptCountsAndKeepsTheItemPending) {
    ASSERT_TRUE(store::enqueue(*connection_, item("a")));

    ASSERT_TRUE(store::recordAttempt(*connection_, "a", "network"));
    ASSERT_TRUE(store::recordAttempt(*connection_, "a", "network"));

    const auto rows = store::all(*connection_);
    ASSERT_TRUE(rows.has_value());
    EXPECT_EQ((*rows)[0].attempts, 2);
    EXPECT_EQ((*rows)[0].status, OutboxStatus::Pending);
    EXPECT_EQ((*rows)[0].lastError, "network");
}

TEST_F(OutboxStoreTest, AParkedItemLeavesThePendingSetButStaysStored) {
    ASSERT_TRUE(store::enqueue(*connection_, item("a")));
    ASSERT_TRUE(store::enqueue(*connection_, item("b")));

    ASSERT_TRUE(store::markFailed(*connection_, "a", "invalid_argument"));

    const auto pending = store::pending(*connection_, 10);
    ASSERT_TRUE(pending.has_value());
    ASSERT_EQ(pending->size(), 1U);
    EXPECT_EQ((*pending)[0].id, "b");
    EXPECT_EQ(store::pendingCount(*connection_).value_or(-1), 1);
    const auto all = store::all(*connection_);
    ASSERT_TRUE(all.has_value());
    ASSERT_EQ(all->size(), 2U);
    EXPECT_EQ((*all)[0].status, OutboxStatus::Failed);
    EXPECT_EQ((*all)[0].lastError, "invalid_argument");
}

TEST_F(OutboxStoreTest, RemoveDeletesOnlyThatItem) {
    ASSERT_TRUE(store::enqueue(*connection_, item("a")));
    ASSERT_TRUE(store::enqueue(*connection_, item("b")));

    ASSERT_TRUE(store::remove(*connection_, "a"));

    const auto rows = store::all(*connection_);
    ASSERT_TRUE(rows.has_value());
    ASSERT_EQ(rows->size(), 1U);
    EXPECT_EQ((*rows)[0].id, "b");
}

TEST_F(OutboxStoreTest, ClearEmptiesTheTableIncludingParkedItems) {
    ASSERT_TRUE(store::enqueue(*connection_, item("a")));
    ASSERT_TRUE(store::enqueue(*connection_, item("b")));
    ASSERT_TRUE(store::markFailed(*connection_, "b", "x"));

    ASSERT_TRUE(store::clear(*connection_));

    const auto rows = store::all(*connection_);
    ASSERT_TRUE(rows.has_value());
    EXPECT_TRUE(rows->empty());
    EXPECT_EQ(store::pendingCount(*connection_).value_or(-1), 0);
}

TEST_F(OutboxStoreTest, AnIdThatWasDeliveredCanBeQueuedAgain) {
    ASSERT_TRUE(store::enqueue(*connection_, item("a")));
    ASSERT_TRUE(store::remove(*connection_, "a"));

    ASSERT_TRUE(store::enqueue(*connection_, item("a")));

    EXPECT_EQ(store::pendingCount(*connection_).value_or(-1), 1);
}

} // namespace
} // namespace aedst::outbox
