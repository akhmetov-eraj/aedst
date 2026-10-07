#pragma once

#include "Domain/task.hpp"
#include "Domain/task_repository.hpp"
#include "aedst/core/use_case.hpp"

#include <memory>
#include <optional>

namespace aedst::feature::tasks {

/// Trims and validates locally (instant feedback, same limits as the proto's
/// buf.validate rules: title 1-200 characters, description <= 4000, a defined
/// priority) before calling CreateTask. The server stays authoritative.
class CreateTaskUseCase final : public core::UseCase<NewTask, Task> {
public:
    static constexpr std::size_t kMaxTitleChars = 200;
    static constexpr std::size_t kMaxDescriptionChars = 4000;

    explicit CreateTaskUseCase(std::shared_ptr<TaskRepository> repository);

    void execute(NewTask input, core::Completion<Task> done) override;

    /// Normalizes `task` in place (trimmed title/description) and returns the
    /// first validation error, if any. Lengths count Unicode code points (UTF-8
    /// lead bytes), like protovalidate's string.max_len.
    [[nodiscard]] static std::optional<core::Error> normalizeAndValidate(NewTask& task);

private:
    std::shared_ptr<TaskRepository> repository_;
};

} // namespace aedst::feature::tasks
