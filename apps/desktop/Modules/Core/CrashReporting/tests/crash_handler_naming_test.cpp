#include "aedst/crash/crash_handler.hpp"

#include <gtest/gtest.h>

#include <chrono>

namespace aedst::crash {
namespace {

using std::chrono::seconds;
using std::chrono::system_clock;

TEST(CrashHandlerNamingTest, FormatsUtcTimestamps) {
    EXPECT_EQ(formatCrashTimestamp(system_clock::time_point{seconds{0}}), "19700101T000000Z");
    EXPECT_EQ(formatCrashTimestamp(system_clock::time_point{seconds{1'700'000'000}}), "20231114T221320Z");
    // Leap day and end of day.
    EXPECT_EQ(formatCrashTimestamp(system_clock::time_point{seconds{951'782'399}}), "20000228T235959Z");
    EXPECT_EQ(formatCrashTimestamp(system_clock::time_point{seconds{951'782'400}}), "20000229T000000Z");
}

TEST(CrashHandlerNamingTest, BuildsCrashFileNames) {
    EXPECT_EQ(crashFileName(system_clock::time_point{seconds{1'700'000'000}}, 4242UL, ".txt"),
              "crash-20231114T221320Z-4242.txt");
    EXPECT_EQ(crashFileName(system_clock::time_point{seconds{0}}, 1UL, ".dmp"), "crash-19700101T000000Z-1.dmp");
}

} // namespace
} // namespace aedst::crash
