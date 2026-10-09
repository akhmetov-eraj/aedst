#pragma once

#include "aedst/core/outcome.hpp"
#include "aedst/outbox/outbox_handler.hpp"
#include "aedst/outbox/outbox_queue.hpp"

#include <memory>

namespace aedst::outbox {

enum class RunResult {
    /// Nothing pending is left (delivered, or parked as failed).
    Complete,
    /// A retryable failure stopped the run; later items were not attempted. Run again after a backoff.
    Retry,
    /// A handler reported Blocked (session ended, upgrade required): stopped without counting an attempt.
    Blocked,
};

struct RunReport {
    RunResult result{RunResult::Complete};
    int delivered{0};
    int parked{0};
};

struct DrainerConfig {
    /// The 8th failed attempt of an item parks it (docs/contract/behavior/offline-outbox.json: maxAttempts).
    int maxAttempts{8};
    /// Items read from the queue per round trip.
    int batchSize{50};
};

/// Delivers the pending outbox items in creation order (docs/contract/behavior/offline-outbox.json):
///  - the item id is the Idempotency-Key of every attempt;
///  - success removes the item;
///  - a retryable failure records an attempt and STOPS the run (`Retry`), later items are not attempted;
///  - a permanent failure, an item type nobody handles, or the maxAttempts-th retryable failure parks the item as
///    failed and the run continues.
/// Framework-free (no Qt): asynchronous through OutboxQueue and the handlers' completions, so it is tested against
/// the real SQLite queue with the shared fixtures. One run at a time per drainer is the caller's job (OutboxService).
/// A storage error from the queue ends the run with that error.
class OutboxDrainer final : public std::enable_shared_from_this<OutboxDrainer> {
public:
    /// Use create(): the drainer keeps itself alive while a run is in flight.
    [[nodiscard]] static std::shared_ptr<OutboxDrainer> create(std::shared_ptr<OutboxQueue> queue,
                                                               std::shared_ptr<const OutboxHandlers> handlers,
                                                               DrainerConfig config = {});

    void run(core::Completion<RunReport> done);

private:
    struct Run;

    OutboxDrainer(std::shared_ptr<OutboxQueue> queue, std::shared_ptr<const OutboxHandlers> handlers,
                  DrainerConfig config);

    void nextBatch(const std::shared_ptr<Run>& run);
    void deliverNext(const std::shared_ptr<Run>& run);
    void onDelivered(const std::shared_ptr<Run>& run, const OutboxItem& item, const DeliveryResult& result);
    void park(const std::shared_ptr<Run>& run, const OutboxItem& item, const std::string& errorKind);
    /// Continues with the next item once `outcome` is ok; a storage error ends the run.
    void proceed(const std::shared_ptr<Run>& run, core::VoidOutcome outcome);
    void finish(const std::shared_ptr<Run>& run, RunResult result);

    std::shared_ptr<OutboxQueue> queue_;
    std::shared_ptr<const OutboxHandlers> handlers_;
    DrainerConfig config_;
};

} // namespace aedst::outbox
