#pragma once

#include "aedst/core/logger.hpp"

namespace aedst::telemetry {

struct QtLogBridgeOptions {
    /// Qt messages at or above this level are copied into AedstLogger (spdlog) as
    /// logger "qt" with their QLoggingCategory, so the support log file also shows
    /// QML binding errors, plugin failures and qt.* warnings. Debug/Info stay out
    /// by default: enabling a chatty qt.* category via QT_LOGGING_RULES must not
    /// flood the product log (the reason the two loggers were split in the first place).
    core::LogLevel forwardFrom{core::LogLevel::Warn};
    /// Warnings and above also become crash-reporter breadcrumbs (context for the
    /// next crash/error event; nothing is sent by itself).
    bool breadcrumbs{true};
};

/// Installs a process-wide qInstallMessageHandler that forwards Qt's
/// qDebug/qWarning/qCritical/qFatal (and QML console/binding messages) into
/// spdlog per `options`, then chains to the previously installed handler so Qt's
/// own stderr/debugger output keeps working unchanged. Both loggers stay (owner
/// decision): AEDST code logs through AedstLogger, Qt code through Qt logging;
/// this bridge only makes the single support log file complete.
/// Re-entrancy-safe (a message logged while handling one is passed straight to
/// the previous handler). Call once, after AedstLogger::init().
void installQtLogBridge(const QtLogBridgeOptions& options = {});

/// Restores the previous handler (clean shutdown, tests).
void uninstallQtLogBridge();

} // namespace aedst::telemetry
