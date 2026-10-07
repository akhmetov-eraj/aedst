#pragma once

#include "Domain/task.hpp"
#include "aedst/core/outcome.hpp"

#include <string>

namespace aedst::feature::tasks {

/// Domain-owned port; Data/ConnectTaskRepository implements it over
/// aedst.tasks.v1.TasksService. Every call is scoped to the signed-in user by
/// the server (user_id is never a request field).
class TaskRepository {
public:
    TaskRepository() = default;
    TaskRepository(const TaskRepository&) = delete;
    TaskRepository& operator=(const TaskRepository&) = delete;
    TaskRepository(TaskRepository&&) = delete;
    TaskRepository& operator=(TaskRepository&&) = delete;
    virtual ~TaskRepository() = default;

    virtual void list(const TaskQuery& query, core::Completion<TaskPage> done) = 0;
    virtual void get(const std::string& id, core::Completion<Task> done) = 0;
    virtual void create(const NewTask& task, core::Completion<Task> done) = 0;
    virtual void complete(const std::string& id, core::Completion<Task> done) = 0;
    virtual void reopen(const std::string& id, core::Completion<Task> done) = 0;
    virtual void remove(const std::string& id, core::Completion<void> done) = 0;
};

} // namespace aedst::feature::tasks
