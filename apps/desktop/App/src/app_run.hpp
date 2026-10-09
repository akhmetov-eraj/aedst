#pragma once

#include "startup_trace.hpp"

#include <QApplication>

#include <filesystem>

namespace aedst::app {

struct RunOptions {
    std::filesystem::path dataDirectory;
    /// Whether Sentry is active (drives the crash-report consent switch).
    bool crashReportingConfigured{false};
    /// `--perf-exit-after-ms`: quit this long after the first frame (perf harness). 0 = never.
    int perfExitAfterMs{0};
};

/// Builds the application objects, runs the event loop and destroys everything again before it returns. main() shuts
/// the telemetry down afterwards, so a crash while the objects are torn down is still reported.
/// Returns the process exit code (0 = clean exit, which also marks this start healthy for the update guard).
[[nodiscard]] int runApplication(QApplication& app, const StartupTrace& startup, const RunOptions& options);

} // namespace aedst::app
