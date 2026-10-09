#pragma once

#include "aedst/core/outcome.hpp"
#include "aedst/outbox/outbox_item.hpp"
#include "aedst/persistence/database_worker.hpp"

#include <memory>
#include <string>
#include <vector>

namespace aedst::outbox {

/// Asynchronous persistence port of the outbox. The drainer and the service only know this interface; the
/// production implementation (SqliteOutboxQueue) runs on the database thread, so no SQL ever touches the GUI thread.
/// Every completion runs exactly once, on the thread the implementation's dispatcher names.
class OutboxQueue {
public:
    OutboxQueue() = default;
    OutboxQueue(const OutboxQueue&) = delete;
    OutboxQueue& operator=(const OutboxQueue&) = delete;
    OutboxQueue(OutboxQueue&&) = delete;
    OutboxQueue& operator=(OutboxQueue&&) = delete;
    virtual ~OutboxQueue() = default;

    /// Duplicate ids are ignored (the stored row wins) and still complete successfully.
    virtual void enqueue(OutboxItem item, core::Completion<void> done) = 0;
    /// Pending items in creation order.
    virtual void pending(int limit, core::Completion<std::vector<OutboxItem>> done) = 0;
    /// Every row, parked ones included (diagnostics, tests).
    virtual void snapshot(core::Completion<std::vector<OutboxItem>> done) = 0;
    virtual void pendingCount(core::Completion<int> done) = 0;
    virtual void remove(std::string id, core::Completion<void> done) = 0;
    virtual void recordAttempt(std::string id, std::string errorKind, core::Completion<void> done) = 0;
    virtual void markFailed(std::string id, std::string errorKind, core::Completion<void> done) = 0;
    /// Deletes everything (sign-out).
    virtual void clear(core::Completion<void> done) = 0;
};

/// OutboxQueue on the app's DatabaseWorker (the same single SQLite thread the token store uses; the worker applies
/// the migrations, including the `outbox` table, before the first job runs).
class SqliteOutboxQueue final : public OutboxQueue {
public:
    SqliteOutboxQueue(std::shared_ptr<persistence::DatabaseWorker> worker, persistence::Dispatcher dispatcher);

    void enqueue(OutboxItem item, core::Completion<void> done) override;
    void pending(int limit, core::Completion<std::vector<OutboxItem>> done) override;
    void snapshot(core::Completion<std::vector<OutboxItem>> done) override;
    void pendingCount(core::Completion<int> done) override;
    void remove(std::string id, core::Completion<void> done) override;
    void recordAttempt(std::string id, std::string errorKind, core::Completion<void> done) override;
    void markFailed(std::string id, std::string errorKind, core::Completion<void> done) override;
    void clear(core::Completion<void> done) override;

private:
    std::shared_ptr<persistence::DatabaseWorker> worker_;
    persistence::Dispatcher dispatcher_;
};

} // namespace aedst::outbox
