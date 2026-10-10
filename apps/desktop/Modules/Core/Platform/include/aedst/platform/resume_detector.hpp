#pragma once

#include <chrono>

namespace aedst::platform {

/// Detects "the machine slept" from a periodic tick, without any OS API.
///
/// A coarse timer is expected to fire every `interval`. When the wall clock advanced by much more than that
/// between two ticks, the process was suspended (system sleep, hibernation, VM pause). Works the same on Windows
/// and Linux; the OS-specific power notifications are deliberately not used (ADR-0036).
///
/// Pure logic: the caller passes the time, so it is deterministic in tests. Not thread-safe.
class ResumeDetector final {
public:
    using Clock = std::chrono::system_clock;

    /// `interval`: the tick period. `tolerance`: how many intervals of lateness still count as a busy event loop.
    explicit constexpr ResumeDetector(std::chrono::milliseconds interval, int tolerance = 3) noexcept
        : interval_(interval), tolerance_(tolerance < 1 ? 1 : tolerance) {}

    /// Call on every tick. Returns true when the gap since the previous tick proves a suspension.
    /// The first call only records the time. A clock moved backwards is not a resume.
    [[nodiscard]] constexpr bool observe(Clock::time_point now) noexcept {
        const bool first = !seen_;
        const auto gap = now - last_;
        last_ = now;
        seen_ = true;
        if (first || gap < Clock::duration::zero()) {
            return false;
        }
        return gap > interval_ * tolerance_;
    }

private:
    std::chrono::milliseconds interval_;
    int tolerance_;
    Clock::time_point last_{};
    bool seen_{false};
};

} // namespace aedst::platform
