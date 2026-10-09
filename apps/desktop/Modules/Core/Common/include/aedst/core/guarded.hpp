#pragma once

#include <exception>
#include <utility>

namespace aedst::core {

/// Logs an exception that escaped caller-supplied code (defined in guarded.cpp; goes through AedstLogger, so the
/// redaction applies).
void logGuardedException(const char* what, const std::exception* e) noexcept;

/// Runs `fn` and contains anything it throws. Qt does not support exceptions leaving a slot (the event loop would
/// terminate the process), so every call into a caller-supplied completion that runs from a Qt slot or timer goes
/// through here: an exception is logged and swallowed, and the next event is delivered normally.
template <typename Fn>
void invokeGuarded(const char* what, Fn&& fn) noexcept {
    try {
        std::forward<Fn>(fn)();
    } catch (const std::exception& e) {
        logGuardedException(what, &e);
    } catch (...) {
        logGuardedException(what, nullptr);
    }
}

} // namespace aedst::core
