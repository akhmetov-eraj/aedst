#pragma once

#include "aedst/core/outcome.hpp"

#include <variant>

namespace aedst::core {

using NoInput = std::monostate;

/// One application operation. Async by construction because every real
/// implementation ends in a network or disk call; tests pass completions that
/// capture the result synchronously.
template <typename Input, typename Output>
class UseCase {
public:
    UseCase() = default;
    UseCase(const UseCase&) = delete;
    UseCase& operator=(const UseCase&) = delete;
    UseCase(UseCase&&) = delete;
    UseCase& operator=(UseCase&&) = delete;
    virtual ~UseCase() = default;

    virtual void execute(Input input, Completion<Output> done) = 0;
};

} // namespace aedst::core
