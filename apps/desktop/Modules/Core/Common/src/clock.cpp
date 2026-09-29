#include "aedst/core/clock.hpp"

namespace aedst::core {

Clock::TimePoint SystemClock::now() const {
    return std::chrono::system_clock::now();
}

} // namespace aedst::core
