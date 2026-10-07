#include "Presentation/task_detail_view_model.hpp"

#include "Domain/task_repository.hpp"
#include "Presentation/task_list_model.hpp"
#include "aedst/core/error_sink.hpp"
#include "aedst/media/media_remote_data_source.hpp"
#include "aedst/navigation/route.hpp"
#include "aedst/navigation/router.hpp"

#include <QPointer>

#include <algorithm>
#include <chrono>
#include <utility>

namespace aedst::feature::tasks {
namespace {

constexpr int kFirstPollMs = 2000;
constexpr int kMaxPollMs = 30'000;
constexpr int kMaxPollAttempts = 20;

} // namespace

TaskDetailViewModel::TaskDetailViewModel(Dependencies dependencies, QObject* parent)
    : QObject(parent), deps_(std::move(dependencies)), uploader_(deps_.upload, this) {
    attachmentPoll_.setSingleShot(true);
    connect(&attachmentPoll_, &QTimer::timeout, this, &TaskDetailViewModel::loadAttachments);
    connect(&uploader_, &media::MediaUploadController::uploaded, this, [this](const QString&) {
        pollAttempts_ = 0;
        loadAttachments();
    });
}

QString TaskDetailViewModel::taskId() const {
    return task_ ? QString::fromStdString(task_->id) : requestedId_;
}

QString TaskDetailViewModel::title() const {
    return task_ ? QString::fromStdString(task_->title) : QString();
}

QString TaskDetailViewModel::description() const {
    return task_ ? QString::fromStdString(task_->description) : QString();
}

QString TaskDetailViewModel::status() const {
    if (!task_) {
        return {};
    }
    const std::string_view name = toString(task_->status);
    return QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size()));
}

bool TaskDetailViewModel::done() const {
    return task_ && task_->isDone();
}

int TaskDetailViewModel::priority() const {
    return task_ ? static_cast<int>(task_->priority) : 0;
}

QDateTime TaskDetailViewModel::dueDate() const {
    return task_ ? TaskListModel::toQDateTime(task_->dueDate) : QDateTime();
}

bool TaskDetailViewModel::overdue() const {
    return task_ && task_->isOverdue(std::chrono::system_clock::now());
}

void TaskDetailViewModel::load(const QString& taskId) {
    if (taskId.isEmpty()) {
        return;
    }
    if (taskId != requestedId_) {
        setTask(std::nullopt);
        attachments_.setItems({});
        uploader_.abortAll();
    }
    requestedId_ = taskId;
    uploader_.setTaskId(taskId);
    reload();
}

void TaskDetailViewModel::reload() {
    if (requestedId_.isEmpty()) {
        return;
    }
    setErrorMessage({});
    setLoading(true);
    const quint64 seq = ++seq_;
    QPointer<TaskDetailViewModel> guard(this);
    deps_.tasks->get(requestedId_.toStdString(), [guard, seq](core::Outcome<Task> result) {
        if (!guard || seq != guard->seq_) {
            return;
        }
        guard->setLoading(false);
        if (!result) {
            guard->setErrorMessage(QString::fromStdString(result.error().message));
            if (guard->deps_.errors) {
                guard->deps_.errors->report(result.error(), "tasks.detail", core::ErrorVisibility::Silent);
            }
            return;
        }
        guard->setTask(std::move(*result));
    });
    pollAttempts_ = 0;
    loadAttachments();
}

void TaskDetailViewModel::loadAttachments() {
    if (requestedId_.isEmpty()) {
        return;
    }
    media::mediav1::ListMediaRequest request;
    request.set_task_id(requestedId_.toStdString());
    request.set_page_size(100);
    const quint64 seq = seq_;
    QPointer<TaskDetailViewModel> guard(this);
    deps_.media->listMedia(request, [guard, seq](network::ApiResult<media::mediav1::ListMediaResponse> result) {
        if (!guard || seq != guard->seq_) {
            return;
        }
        if (!result) {
            if (guard->deps_.errors) {
                guard->deps_.errors->report(result.error().toCoreError(), "tasks.attachments",
                                            core::ErrorVisibility::Silent);
            }
            guard->scheduleAttachmentPoll(); // retry with backoff; the page stays usable
            return;
        }
        std::vector<media::mediav1::Media> items(result->media().begin(), result->media().end());
        guard->attachments_.setItems(std::move(items));
        if (guard->attachments_.hasUnfinished()) {
            guard->scheduleAttachmentPoll();
        }
    });
}

void TaskDetailViewModel::scheduleAttachmentPoll() {
    if (pollAttempts_ >= kMaxPollAttempts) {
        return;
    }
    const int delay = std::min(kMaxPollMs, kFirstPollMs * (1 << std::min(pollAttempts_, 4)));
    ++pollAttempts_;
    attachmentPoll_.start(delay);
}

void TaskDetailViewModel::setDone(bool done) {
    if (!task_ || task_->isDone() == done || isMutating_) {
        return;
    }
    setMutating(true);
    QPointer<TaskDetailViewModel> guard(this);
    auto onDone = [guard, done](core::Outcome<Task> result) {
        if (!guard) {
            return;
        }
        guard->setMutating(false);
        if (!result) {
            if (guard->deps_.errors) {
                guard->deps_.errors->report(result.error(), done ? "tasks.complete" : "tasks.reopen");
            }
            return;
        }
        guard->setTask(std::move(*result));
    };
    if (done) {
        deps_.tasks->complete(task_->id, std::move(onDone));
    } else {
        deps_.tasks->reopen(task_->id, std::move(onDone));
    }
}

void TaskDetailViewModel::deleteTask() {
    if (!task_ || isMutating_) {
        return;
    }
    setMutating(true);
    QPointer<TaskDetailViewModel> guard(this);
    deps_.tasks->remove(task_->id, [guard](core::VoidOutcome result) {
        if (!guard) {
            return;
        }
        guard->setMutating(false);
        if (!result) {
            if (guard->deps_.errors) {
                guard->deps_.errors->report(result.error(), "tasks.delete");
            }
            return;
        }
        guard->clear();
        emit guard->taskDeleted();
        if (guard->deps_.router != nullptr) {
            guard->deps_.router->goBack();
        }
    });
}

void TaskDetailViewModel::deleteAttachment(const QString& mediaId) {
    media::mediav1::DeleteMediaRequest request;
    request.set_id(mediaId.toStdString());
    QPointer<TaskDetailViewModel> guard(this);
    deps_.media->deleteMedia(request, [guard, mediaId](network::ApiResult<void> result) {
        if (!guard) {
            return;
        }
        if (!result) {
            if (guard->deps_.errors) {
                guard->deps_.errors->report(result.error().toCoreError(), "tasks.attachments.delete");
            }
            return;
        }
        guard->attachments_.remove(mediaId);
    });
}

void TaskDetailViewModel::openAttachment(const QString& mediaId) {
    if (deps_.router == nullptr || mediaId.isEmpty()) {
        return;
    }
    deps_.router->push(navigation::Route{
        .name = std::string(navigation::routes::kMediaViewer),
        .params = {{std::string(navigation::params::kMediaId), mediaId.toStdString()}},
    });
}

void TaskDetailViewModel::clear() {
    ++seq_;
    attachmentPoll_.stop();
    uploader_.abortAll();
    requestedId_.clear();
    attachments_.setItems({});
    setTask(std::nullopt);
    setErrorMessage({});
    setLoading(false);
}

void TaskDetailViewModel::setTask(std::optional<Task> task) {
    if (task == task_) {
        return;
    }
    task_ = std::move(task);
    emit taskChanged();
}

void TaskDetailViewModel::setLoading(bool loading) {
    if (loading != isLoading_) {
        isLoading_ = loading;
        emit isLoadingChanged();
    }
}

void TaskDetailViewModel::setMutating(bool mutating) {
    if (mutating != isMutating_) {
        isMutating_ = mutating;
        emit isMutatingChanged();
    }
}

void TaskDetailViewModel::setErrorMessage(const QString& message) {
    if (message != errorMessage_) {
        errorMessage_ = message;
        emit errorMessageChanged();
    }
}

} // namespace aedst::feature::tasks
