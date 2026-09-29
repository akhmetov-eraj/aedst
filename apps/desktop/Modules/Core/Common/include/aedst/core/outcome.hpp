#pragma once

#include "aedst/core/error.hpp"

#include <expected>
#include <functional>
#include <utility>

namespace aedst::core {

/// Result type used across every layer boundary (Swift's Result / Kotlin's
/// Result equivalent). Exceptions are not used for expected failures.
template <typename T>
using Outcome = std::expected<T, Error>;

using VoidOutcome = Outcome<void>;

/// Async continuation. Qt Network completes on the GUI thread's event loop,
/// so completions run there too — no extra marshalling needed in ViewModels.
template <typename T>
using Completion = std::function<void(Outcome<T>)>;

[[nodiscard]] inline std::unexpected<Error> fail(Error error) {
    return std::unexpected<Error>(std::move(error));
}

[[nodiscard]] inline VoidOutcome ok() {
    return {};
}

} // namespace aedst::core
