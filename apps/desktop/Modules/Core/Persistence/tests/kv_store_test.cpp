#include "aedst/persistence/kv_store.hpp"
#include "aedst/persistence/migrator.hpp"
#include "aedst/persistence/sqlite_connection.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace aedst::persistence {
namespace {

class KvStoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto connection = SqliteConnection::openInMemory();
        ASSERT_TRUE(connection.has_value());
        connection_ = std::make_unique<SqliteConnection>(std::move(*connection));
        ASSERT_TRUE(Migrator(appMigrations()).migrate(*connection_).has_value());
    }

    std::unique_ptr<SqliteConnection> connection_;
};

TEST_F(KvStoreTest, MissingKeyIsEmpty) {
    auto value = kv::get(*connection_, "capabilities");
    ASSERT_TRUE(value.has_value());
    EXPECT_FALSE(value->has_value());
}

TEST_F(KvStoreTest, PutThenGetRoundTripsBinaryBytes) {
    const std::array<std::uint8_t, 4> bytes{0x00, 0xFF, 0x10, 0x00}; // embedded NULs must survive
    ASSERT_TRUE(kv::put(*connection_, "capabilities", bytes, 1));

    auto value = kv::get(*connection_, "capabilities");
    ASSERT_TRUE(value.has_value());
    ASSERT_TRUE(value->has_value());
    EXPECT_EQ(**value, std::vector<std::uint8_t>(bytes.begin(), bytes.end()));
}

TEST_F(KvStoreTest, PutReplacesAndRemoveDeletes) {
    const std::array<std::uint8_t, 1> first{1};
    const std::array<std::uint8_t, 1> second{2};
    ASSERT_TRUE(kv::put(*connection_, "k", first, 1));
    ASSERT_TRUE(kv::put(*connection_, "k", second, 2));
    EXPECT_EQ(**kv::get(*connection_, "k"), std::vector<std::uint8_t>{2});

    ASSERT_TRUE(kv::remove(*connection_, "k"));
    EXPECT_FALSE(kv::get(*connection_, "k")->has_value());
}

} // namespace
} // namespace aedst::persistence
