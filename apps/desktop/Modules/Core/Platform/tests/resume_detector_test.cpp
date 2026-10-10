#include "aedst/platform/resume_detector.hpp"

#include <gtest/gtest.h>

#include <chrono>

namespace aedst::platform {
namespace {

using namespace std::chrono_literals;

const ResumeDetector::Clock::time_point kStart{std::chrono::hours{1'000}};

TEST(ResumeDetectorTest, FirstTickIsABaselineNotAResume) {
    ResumeDetector detector(15s);
    EXPECT_FALSE(detector.observe(kStart));
}

TEST(ResumeDetectorTest, OnTimeTicksAreNotAResume) {
    ResumeDetector detector(15s);
    (void)detector.observe(kStart);
    EXPECT_FALSE(detector.observe(kStart + 15s));
    EXPECT_FALSE(detector.observe(kStart + 30s));
}

TEST(ResumeDetectorTest, BusyEventLoopWithinToleranceIsNotAResume) {
    ResumeDetector detector(15s, 3);
    (void)detector.observe(kStart);
    EXPECT_FALSE(detector.observe(kStart + 45s)); // exactly 3 intervals: still tolerated
}

TEST(ResumeDetectorTest, LongGapIsAResumeOnce) {
    ResumeDetector detector(15s, 3);
    (void)detector.observe(kStart);
    EXPECT_TRUE(detector.observe(kStart + 8h));
    EXPECT_FALSE(detector.observe(kStart + 8h + 15s)); // back on schedule
}

TEST(ResumeDetectorTest, ClockMovedBackwardsIsNotAResume) {
    ResumeDetector detector(15s);
    (void)detector.observe(kStart);
    EXPECT_FALSE(detector.observe(kStart - 1h));
}

TEST(ResumeDetectorTest, ToleranceBelowOneIsClamped) {
    ResumeDetector detector(15s, 0);
    (void)detector.observe(kStart);
    EXPECT_TRUE(detector.observe(kStart + 16s));
}

} // namespace
} // namespace aedst::platform
