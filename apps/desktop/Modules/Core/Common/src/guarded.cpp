#include "aedst/core/guarded.hpp"

#include "aedst/core/logger.hpp"

namespace aedst::core {

void logGuardedException(const char* what, const std::exception* e) noexcept {
    try {
        if (e != nullptr) {
            AedstLogger::critical("{} threw an exception: {}", what, e->what());
        } else {
            AedstLogger::critical("{} threw a non-standard exception", what);
        }
    } catch (...) {
        // Logging must never throw out of a noexcept handler.
    }
}

} // namespace aedst::core
