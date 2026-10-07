#pragma once

#include "aedst/core/logger.hpp"
#include "aedst/telemetry/sentry_reporter.hpp"

#include <string_view>

namespace aedst::telemetry {

struct QtLogBridgeOptions {
    /// Messages of first-party categories (`aedst.*`, see aedst/logging/log_categories.hpp)
    /// are always forwarded; spdlog's configured level decides what is written.
    ///
    /// Other Qt messages (qt.*, qml, default) at or above this level are copied into AedstLogger (spdlog) as
    /// logger "qt" with their QLoggingCategory, so the support log file also shows
    /// QML binding errors, plugin failures and qt.* warnings. Debug/Info stay out
    /// by default: enabling a chatty qt.* category via QT_LOGGING_RULES must not
    /// flood the product log (the reason the two loggers were split in the first place).
    core::LogLevel forwardFrom{core::LogLevel::Warn};
    /// Crash-reporter breadcrumbs (context for the next crash/error event; nothing is sent by itself): warnings and
    /// above from every category, and Info from first-party `aedst.*` categories (sign-in, navigation, updates,
    /// connectivity: the trail that explains a crash). Debug never. Text is scrubbed (secrets, e-mails, account
    /// names in paths) before it is stored.
    bool breadcrumbs{true};
    /// Where breadcrumbs go. Null = SentryReporter::addBreadcrumb (a no-op without Sentry). Tests set their own.
    using BreadcrumbSink = void (*)(std::string_view category, std::string_view message, BreadcrumbLevel level);
    BreadcrumbSink breadcrumbSink{nullptr};
};

/// Installs a process-wide qInstallMessageHandler that forwards Qt's
/// qDebug/qWarning/qCritical/qFatal (and QML console/binding messages) into
/// spdlog per `options`, then chains to the previously installed handler so Qt's
/// own stderr/debugger output keeps working unchanged. Qt-aware AEDST code logs
/// through QLoggingCategory (`lcNetwork`, ...), Qt-free code through AedstLogger;
/// spdlog (rotation, JSON, redaction) is the one sink behind both.
/// Re-entrancy-safe (a message logged while handling one is passed straight to
/// the previous handler). Call once, after AedstLogger::init().
void installQtLogBridge(const QtLogBridgeOptions& options = {});

/// Restores the previous handler (clean shutdown, tests).
void uninstallQtLogBridge();

} // namespace aedst::telemetry
