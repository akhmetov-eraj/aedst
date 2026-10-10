#pragma once

#include "aedst/platform/resume_detector.hpp"

#include <QObject>
#include <QTimer>

#include <chrono>

namespace aedst::platform {

/// Tells the rest of the app that the machine woke up, so sockets that died while it slept are replaced at once
/// instead of after their idle watchdog (90 s for the notification stream) or a retry backoff.
///
/// One very coarse tick (15 s by default, `Qt::VeryCoarseTimer`: the OS may batch it with other wake-ups, so it
/// does not keep the CPU out of deep idle). GUI thread only.
class PowerMonitor final : public QObject {
    Q_OBJECT

public:
    explicit PowerMonitor(std::chrono::milliseconds interval = std::chrono::seconds{15}, QObject* parent = nullptr);

    void start();
    void stop();

    /// Test seam: feeds one tick with an explicit time (what the timer does with `now()`).
    void tick(ResumeDetector::Clock::time_point now);

signals:
    /// The process was suspended for longer than a few tick intervals and is running again.
    void resumedFromSleep();

private:
    QTimer timer_;
    ResumeDetector detector_;
};

} // namespace aedst::platform
