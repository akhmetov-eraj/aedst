#include "Data/connect_task_repository.hpp"

#include "aedst/contract/methods.hpp"
#include "aedst/contract/timestamps.hpp"
#include "aedst/network/connect_client.hpp"

#include <utility>

namespace aedst::feature::tasks {
namespace {

[[nodiscard]] TaskStatus toDomain(tasksv1::TaskStatus status) {
    switch (status) {
    case tasksv1::TASK_STATUS_TODO:
        return TaskStatus::Todo;
    case tasksv1::TASK_STATUS_IN_PROGRESS:
        return TaskStatus::InProgress;
    case tasksv1::TASK_STATUS_DONE:
        return TaskStatus::Done;
    default:
        break;
    }
    return TaskStatus::Unspecified;
}

[[nodiscard]] TaskPriority toDomain(tasksv1::TaskPriority priority) {
    switch (priority) {
    case tasksv1::TASK_PRIORITY_LOW:
        return TaskPriority::Low;
    case tasksv1::TASK_PRIORITY_MEDIUM:
        return TaskPriority::Medium;
    case tasksv1::TASK_PRIORITY_HIGH:
        return TaskPriority::High;
    case tasksv1::TASK_PRIORITY_URGENT:
        return TaskPriority::Urgent;
    default:
        break;
    }
    return TaskPriority::Unspecified;
}

[[nodiscard]] tasksv1::TaskPriority toProto(TaskPriority priority) {
    switch (priority) {
    case TaskPriority::Low:
        return tasksv1::TASK_PRIORITY_LOW;
    case TaskPriority::High:
        return tasksv1::TASK_PRIORITY_HIGH;
    case TaskPriority::Urgent:
        return tasksv1::TASK_PRIORITY_URGENT;
    case TaskPriority::Medium:
    case TaskPriority::Unspecified:
        break;
    }
    return tasksv1::TASK_PRIORITY_MEDIUM;
}

/// Response with a `task` field -> Outcome<Task>; an empty task is a Decoding error.
template <typename Response>
network::ApiCompletion<Response> mapTask(core::Completion<Task> done) {
    return [done = std::move(done)](network::ApiResult<Response> result) {
        if (!result) {
            done(core::fail(result.error().toCoreError()));
            return;
        }
        if (!result->has_task() || result->task().id().empty()) {
            done(core::fail(core::Error::unknown("Incomplete task in response")));
            return;
        }
        done(ConnectTaskRepository::toDomain(result->task()));
    };
}

} // namespace

ConnectTaskRepository::ConnectTaskRepository(std::shared_ptr<network::ConnectClient> client)
    : client_(std::move(client)) {}

Task ConnectTaskRepository::toDomain(const tasksv1::Task& task) {
    return Task{
        .id = task.id(),
        .title = task.title(),
        .description = task.description(),
        .status = tasks::toDomain(task.status()),
        .priority = tasks::toDomain(task.priority()),
        .dueDate = contract::toOptionalTimePoint(task.has_due_date(), task.due_date()),
        .createdAt = contract::toTimePoint(task.created_at()),
        .updatedAt = contract::toTimePoint(task.updated_at()),
        .completedAt = contract::toOptionalTimePoint(task.has_completed_at(), task.completed_at()),
        .version = task.version(),
    };
}

void ConnectTaskRepository::list(const TaskQuery& query, core::Completion<TaskPage> done) {
    tasksv1::ListTasksRequest request;
    request.set_search(query.search);
    request.set_page_size(query.pageSize);
    request.set_page_token(query.pageToken);
    if (!query.includeDone) {
        request.add_statuses(tasksv1::TASK_STATUS_TODO);
        request.add_statuses(tasksv1::TASK_STATUS_IN_PROGRESS);
    }
    client_->unary(contract::kTasksListTasks, request,
                   [done = std::move(done)](network::ApiResult<tasksv1::ListTasksResponse> result) {
                       if (!result) {
                           done(core::fail(result.error().toCoreError()));
                           return;
                       }
                       TaskPage page;
                       page.tasks.reserve(static_cast<std::size_t>(result->tasks_size()));
                       for (const auto& task : result->tasks()) {
                           page.tasks.push_back(toDomain(task));
                       }
                       page.nextPageToken = result->next_page_token();
                       page.totalCount = result->total_count();
                       done(std::move(page));
                   });
}

void ConnectTaskRepository::get(const std::string& id, core::Completion<Task> done) {
    tasksv1::GetTaskRequest request;
    request.set_id(id);
    client_->unary(contract::kTasksGetTask, request, mapTask<tasksv1::GetTaskResponse>(std::move(done)));
}

void ConnectTaskRepository::create(const NewTask& task, core::Completion<Task> done) {
    tasksv1::CreateTaskRequest request;
    request.set_title(task.title);
    request.set_description(task.description);
    request.set_priority(toProto(task.priority));
    if (task.dueDate) {
        contract::setTimestamp(*request.mutable_due_date(), *task.dueDate);
    }
    client_->unary(contract::kTasksCreateTask, request, mapTask<tasksv1::CreateTaskResponse>(std::move(done)),
                   network::CallOptions{.idempotencyKey = task.clientRequestId});
}

void ConnectTaskRepository::setDone(const std::string& id, bool done, const std::string& idempotencyKey,
                                    core::Completion<Task> completion) {
    const network::CallOptions options{.idempotencyKey = idempotencyKey};
    if (done) {
        tasksv1::CompleteTaskRequest request;
        request.set_id(id);
        client_->unary(contract::kTasksCompleteTask, request,
                       mapTask<tasksv1::CompleteTaskResponse>(std::move(completion)), options);
        return;
    }
    tasksv1::ReopenTaskRequest request;
    request.set_id(id);
    client_->unary(contract::kTasksReopenTask, request, mapTask<tasksv1::ReopenTaskResponse>(std::move(completion)),
                   options);
}

void ConnectTaskRepository::complete(const std::string& id, core::Completion<Task> done) {
    tasksv1::CompleteTaskRequest request;
    request.set_id(id);
    client_->unary(contract::kTasksCompleteTask, request, mapTask<tasksv1::CompleteTaskResponse>(std::move(done)));
}

void ConnectTaskRepository::reopen(const std::string& id, core::Completion<Task> done) {
    tasksv1::ReopenTaskRequest request;
    request.set_id(id);
    client_->unary(contract::kTasksReopenTask, request, mapTask<tasksv1::ReopenTaskResponse>(std::move(done)));
}

void ConnectTaskRepository::remove(const std::string& id, core::Completion<void> done) {
    tasksv1::DeleteTaskRequest request;
    request.set_id(id);
    client_->unary(contract::kTasksDeleteTask, request,
                   [done = std::move(done)](network::ApiResult<tasksv1::DeleteTaskResponse> result) {
                       if (!result) {
                           done(core::fail(result.error().toCoreError()));
                           return;
                       }
                       done(core::ok());
                   });
}

} // namespace aedst::feature::tasks
