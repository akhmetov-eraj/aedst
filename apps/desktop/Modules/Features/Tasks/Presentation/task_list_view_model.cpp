#include "Presentation/task_list_view_model.hpp"

#include "Domain/create_task_use_case.hpp"
#include "Domain/task_repository.hpp"
#include "Navigation/tasks_route.hpp"
#include "aedst/analytics/analytics_tracker.hpp"
#include "aedst/core/error_sink.hpp"
#include "aedst/navigation/router.hpp"

#include <QPointer>

#include <algorithm>
#include <utility>

namespace aedst::feature::tasks {

TaskListViewModel::TaskListViewModel(std::shared_ptr<TaskRepository> repository,
                                     std::shared_ptr<CreateTaskUseCase> createTask,
                                     std::shared_ptr<core::ErrorSink> errors,
                                     std::shared_ptr<analytics::AnalyticsTracker> analytics, navigation::Router* router,
                                     QObject* parent)
    : QObject(parent), repository_(std::move(repository)), createTask_(std::move(createTask)),
      errors_(std::move(errors)), analytics_(std::move(analytics)), router_(router) {
    filterDebounce_.setSingleShot(true);
    filterDebounce_.setInterval(kFilterDebounceMs);
    connect(&filterDebounce_, &QTimer::timeout, this, &TaskListViewModel::reload);
}

void TaskListViewModel::setFilter(const QString& filter) {
    if (filter == filter_) {
        return;
    }
    filter_ = filter;
    emit filterChanged();
    filterDebounce_.start(); // restarts on every keystroke: one request after typing pauses
}

void TaskListViewModel::setShowCompleted(bool show) {
    if (show == showCompleted_) {
        return;
    }
    showCompleted_ = show;
    emit showCompletedChanged();
    reload();
}

void TaskListViewModel::reload() {
    filterDebounce_.stop();
    load(false);
}

void TaskListViewModel::loadMore() {
    if (!hasMore() || isLoading_) {
        return;
    }
    load(true);
}

void TaskListViewModel::load(bool append) {
    setErrorMessage({});
    setLoading(true);
    const quint64 seq = ++requestSeq_;
    TaskQuery query{.search = filter_.trimmed().left(200).toStdString(),
                    .includeDone = showCompleted_,
                    .pageToken = append ? nextPageToken_ : std::string(),
                    .pageSize = 50};
    QPointer<TaskListViewModel> guard(this);
    repository_->list(query, [guard, seq, append](core::Outcome<TaskPage> result) {
        if (!guard || seq != guard->requestSeq_) {
            return; // destroyed, or superseded by a newer filter
        }
        guard->setLoading(false);
        if (!result) {
            guard->setErrorMessage(QString::fromStdString(result.error().message));
            if (guard->errors_) {
                guard->errors_->report(result.error(), "tasks.list",
                                       append ? core::ErrorVisibility::Transient : core::ErrorVisibility::Silent);
            }
            return;
        }
        if (append) {
            guard->model_.appendTasks(std::move(result->tasks));
        } else {
            guard->model_.setTasks(std::move(result->tasks));
            if (guard->totalCount_ != result->totalCount) {
                guard->totalCount_ = result->totalCount;
                emit guard->totalCountChanged();
            }
        }
        guard->setNextPageToken(std::move(result->nextPageToken));
    });
}

void TaskListViewModel::openTask(const QString& taskId) {
    if (router_ != nullptr && !taskId.isEmpty()) {
        router_->push(taskDetailRoute(taskId.toStdString()));
    }
}

void TaskListViewModel::setDone(const QString& taskId, bool done) {
    const std::string id = taskId.toStdString();
    const auto current = model_.find(id);
    if (!current || current->isDone() == done) {
        return;
    }
    // Optimistic: flip the row now, roll back if the server refuses.
    Task optimistic = *current;
    optimistic.status = done ? TaskStatus::Done : TaskStatus::Todo;
    model_.update(optimistic);

    QPointer<TaskListViewModel> guard(this);
    auto onDone = [guard, previous = *current, done](core::Outcome<Task> result) {
        if (!guard) {
            return;
        }
        if (!result) {
            guard->model_.update(previous);
            if (guard->errors_) {
                guard->errors_->report(result.error(), done ? "tasks.complete" : "tasks.reopen");
            }
            return;
        }
        guard->model_.update(*result);
        if (done && guard->analytics_) {
            guard->analytics_->track(analytics::events::kTaskCompleted, {});
        }
    };
    if (done) {
        repository_->complete(id, std::move(onDone));
    } else {
        repository_->reopen(id, std::move(onDone));
    }
}

void TaskListViewModel::createTask(const QString& title, const QString& description, int priority) {
    if (isCreating_) {
        return;
    }
    isCreating_ = true;
    emit isCreatingChanged();
    NewTask input{.title = title.toStdString(),
                  .description = description.toStdString(),
                  .priority = static_cast<TaskPriority>(std::clamp(priority, 0, 4)),
                  .dueDate = std::nullopt};
    QPointer<TaskListViewModel> guard(this);
    createTask_->execute(std::move(input), [guard](core::Outcome<Task> result) {
        if (!guard) {
            return;
        }
        guard->isCreating_ = false;
        emit guard->isCreatingChanged();
        if (!result) {
            const QString message = QString::fromStdString(result.error().message);
            if (result.error().code != core::ErrorCode::InvalidArgument && guard->errors_) {
                guard->errors_->report(result.error(), "tasks.create");
            }
            emit guard->createFailed(message);
            return;
        }
        const QString id = QString::fromStdString(result->id);
        guard->model_.prepend(std::move(*result));
        ++guard->totalCount_;
        emit guard->totalCountChanged();
        if (guard->analytics_) {
            guard->analytics_->track(analytics::events::kTaskCreated, {});
        }
        emit guard->taskCreated(id);
    });
}

void TaskListViewModel::clear() {
    ++requestSeq_;
    filterDebounce_.stop();
    model_.setTasks({});
    setNextPageToken({});
    setErrorMessage({});
    setLoading(false);
    if (!filter_.isEmpty()) {
        filter_.clear();
        emit filterChanged();
    }
    if (totalCount_ != 0) {
        totalCount_ = 0;
        emit totalCountChanged();
    }
}

void TaskListViewModel::setLoading(bool loading) {
    if (loading != isLoading_) {
        isLoading_ = loading;
        emit isLoadingChanged();
    }
}

void TaskListViewModel::setErrorMessage(const QString& message) {
    if (message != errorMessage_) {
        errorMessage_ = message;
        emit errorMessageChanged();
    }
}

void TaskListViewModel::setNextPageToken(std::string token) {
    const bool before = hasMore();
    nextPageToken_ = std::move(token);
    if (before != hasMore()) {
        emit hasMoreChanged();
    }
}

} // namespace aedst::feature::tasks
