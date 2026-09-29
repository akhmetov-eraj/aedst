#pragma once

#include <chrono>

namespace aedst::core {

/// Injected wherever "now" affects behaviour (token expiry, cache staleness)
/// so tests can use aedst::testing::FakeClock.
class Clock {
public:
    using TimePoint = std::chrono::system_clock::time_point;

    Clock() = default;
    Clock(const Clock&) = delete;
    Clock& operator=(const Clock&) = delete;
    Clock(Clock&&) = delete;
    Clock& operator=(Clock&&) = delete;
    virtual ~Clock() = default;

    [[nodiscard]] virtual TimePoint now() const = 0;
};

class SystemClock final : public Clock {
public:
    [[nodiscard]] TimePoint now() const override;
};

} // namespace aedst::core
