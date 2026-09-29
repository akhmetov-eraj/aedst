#pragma once

#include "aedst/core/clock.hpp"

#include <chrono>

namespace aedst::testing {

/// Deterministic clock: starts at a fixed instant and only moves when told to.
class FakeClock final : public core::Clock {
public:
    explicit FakeClock(TimePoint start = TimePoint{std::chrono::seconds{1'700'000'000}}) : now_(start) {}

    [[nodiscard]] TimePoint now() const override { return now_; }

    void advance(std::chrono::system_clock::duration by) { now_ += by; }

    void set(TimePoint to) { now_ = to; }

private:
    TimePoint now_;
};

} // namespace aedst::testing
