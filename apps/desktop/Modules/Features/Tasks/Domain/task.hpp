#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aedst::feature::tasks {

/// Mirrors aedst.tasks.v1.TaskStatus. Status only moves through Complete/Reopen.
enum class TaskStatus {
    Unspecified,
    Todo,
    InProgress,
    Done,
};

enum class TaskPriority {
    Unspecified,
    Low,
    Medium,
    High,
    Urgent,
};

[[nodiscard]] constexpr std::string_view toString(TaskStatus status) noexcept {
    switch (status) {
    case TaskStatus::Todo:
        return "todo";
    case TaskStatus::InProgress:
        return "in_progress";
    case TaskStatus::Done:
        return "done";
    case TaskStatus::Unspecified:
        break;
    }
    return "unspecified";
}

/// The task as the desktop UI needs it. Domain-owned (pure C++, no protobuf):
/// Data/ maps aedst.tasks.v1.Task into it, so use cases stay testable with plain
/// GoogleTest - the same split Features/Auth makes with Session.
struct Task {
    std::string id;
    std::string title;
    std::string description;
    TaskStatus status{TaskStatus::Unspecified};
    TaskPriority priority{TaskPriority::Unspecified};
    std::optional<std::chrono::system_clock::time_point> dueDate;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point updatedAt;
    std::optional<std::chrono::system_clock::time_point> completedAt;
    /// Optimistic-concurrency version (UpdateTask.expected_version).
    int version{0};

    [[nodiscard]] bool isDone() const noexcept { return status == TaskStatus::Done; }

    [[nodiscard]] bool isOverdue(std::chrono::system_clock::time_point now) const noexcept {
        return !isDone() && dueDate.has_value() && *dueDate < now;
    }

    friend bool operator==(const Task&, const Task&) = default;
};

struct TaskQuery {
    /// Case-insensitive substring match on title + description (server-side).
    std::string search;
    bool includeDone{true};
    std::string pageToken;
    int pageSize{50};
};

struct TaskPage {
    std::vector<Task> tasks;
    std::string nextPageToken;
    /// Only on the first page (see ListTasksResponse.total_count).
    int totalCount{0};
};

struct NewTask {
    std::string title;
    std::string description;
    TaskPriority priority{TaskPriority::Medium};
    std::optional<std::chrono::system_clock::time_point> dueDate;
};

} // namespace aedst::feature::tasks
