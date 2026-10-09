#pragma once

#include "aedst/core/clock.hpp"
#include "aedst/core/outcome.hpp"
#include "aedst/network/call_options.hpp"
#include "aedst/outbox/outbox_drainer.hpp"
#include "aedst/outbox/outbox_handler.hpp"
#include "aedst/outbox/outbox_queue.hpp"

#include <QObject>
#include <QTimer>

#include <chrono>
#include <memory>
#include <string>

namespace aedst::outbox {

/// The Qt face of the offline write path: enqueue, trigger, back off, purge, and the `pendingCount` that drives the
/// "waiting to sync" indicators. GUI thread only.
///
/// When a run happens:
///  - right after enqueue(), and at start() for items left by a previous session;
///  - when the host reports the network is back (setOnline(true), wired to ConnectivityMonitor in App/);
///  - after a retryable failure, once the exponential backoff (RetryPolicy, equal jitter) has elapsed.
/// While offline nothing is attempted. At most one run is in flight; a trigger during a run schedules exactly one more.
/// A run that ends `Blocked` (session ended, upgrade required) schedules nothing: the next trigger tries again.
class OutboxService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int pendingCount READ pendingCount NOTIFY pendingCountChanged)

public:
    struct Dependencies {
        std::shared_ptr<OutboxQueue> queue;
        std::shared_ptr<const OutboxHandlers> handlers;
        std::shared_ptr<core::Clock> clock;
        DrainerConfig drainer{};
        /// First retry after ~2 s, doubling up to 5 min, "equal jitter" (never below half the delay).
        network::RetryPolicy backoff{.maxAttempts = 0,
                                     .initialBackoff = std::chrono::seconds{2},
                                     .maxBackoff = std::chrono::minutes{5},
                                     .multiplier = 2.0};
    };

    explicit OutboxService(Dependencies dependencies, QObject* parent = nullptr);

    [[nodiscard]] int pendingCount() const noexcept { return pendingCount_; }

    /// Persists the operation, then tries to deliver. `id` becomes the Idempotency-Key: pass a stable id (the same
    /// one a direct call used) so a double tap or a retry after a lost response cannot apply it twice; an existing id
    /// is ignored. `done` reports only the local write.
    void enqueue(std::string type, std::string payload, std::string id, core::Completion<void> done = {});

    /// Reads the pending count and starts a first run (items from a previous session).
    void start();

    /// Reachability from the host; going online triggers a run, going offline stops the backoff timer.
    void setOnline(bool online);

    /// Runs now if online and idle (otherwise one run is queued behind the current one).
    void kick();

    /// Sign-out: forgets every queued operation (a different user must never receive them), stops timers and ignores
    /// the result of a run that is still in flight.
    void purge(core::Completion<void> done = {});

signals:
    void pendingCountChanged();
    /// A run delivered at least one item (emitted after pendingCount was refreshed): lists that show server data should
    /// reload once pendingCount() is 0.
    void delivered();

private:
    void startRun();
    void onRunFinished(quint64 generation, const core::Outcome<RunReport>& report);
    void scheduleRetry();
    /// `announceDelivery`: emit delivered() once the fresh count is in, so listeners see a consistent pendingCount.
    void refreshPendingCount(bool announceDelivery = false);
    void setPendingCount(int count);

    Dependencies deps_;
    std::shared_ptr<OutboxDrainer> drainer_;
    QTimer retryTimer_;
    int pendingCount_{0};
    int retryStreak_{0};
    bool online_{true};
    bool running_{false};
    bool rerun_{false};
    /// Bumped by purge(): a run that started before it must not touch the state of the one after.
    quint64 generation_{0};
};

} // namespace aedst::outbox
