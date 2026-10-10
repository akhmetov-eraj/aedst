# ADR-0036: Power and resume handling

- Status: accepted; **not compiled or run** (no Qt/CMake toolchain in the authoring environment). `resume_detector_test.cpp` is written, not executed.
- Scope: `Modules/Core/Platform` (`ResumeDetector`, `PowerMonitor`), `Modules/Features/Notifications` (`NotificationRealtime::reconnectNow`,
  `NotificationsViewModel::onSystemResumed`), `App/src/app_run.cpp` (`wirePowerResume`).

## Problem

After system sleep the notification stream and any in-flight request are dead sockets, yet nothing reports it. The stream noticed only
through its 90 s idle watchdog, and the outbox waited for its next backoff step (up to 5 min). Reachability may not change at all
(same Wi-Fi, same address), so `ConnectivityMonitor` does not help.

## Decisions

1. **Detect resume from clock drift, not from an OS API.** `ResumeDetector` (pure, Qt-free, deterministic) compares the wall-clock gap
   between two ticks with the tick interval; a gap above `interval * tolerance` (default 3) is a suspension. It behaves the same on Windows
   and Linux and needs no `WM_POWERBROADCAST` window, no logind D-Bus subscription and no native event filter. A clock moved backwards is
   not a resume. Revisit when: a platform needs the *before-sleep* event (to flush state), which drift cannot give.
2. **One very coarse tick.** `PowerMonitor` uses `Qt::VeryCoarseTimer` at 15 s, so the OS may batch it with other wake-ups and the CPU can
   stay in deep idle. It runs for the whole session; its cost is one wake-up per 15 s at most.
3. **What a resume does**, all on the GUI thread: `NotificationsViewModel::onSystemResumed()` (replace the stream now with
   `NotificationRealtime::reconnectNow()`, reset the failure count because the outage was the machine's, poll once), `OutboxService::kick()`
   and `CapabilitiesService::refresh()`. Each is a no-op when signed out, offline or already running a request.
4. **The stream is not paused in the background.** A tray app must keep delivering notifications while minimized; Qt Quick already stops
   rendering for a hidden window. The timers that remain are single-shot backoff/watchdog timers.

## Not decided here

- Battery-saver detection (Windows `SystemPowerStatus`, UPower `OnBattery`) to lengthen the poll interval: needs a measurement first.
