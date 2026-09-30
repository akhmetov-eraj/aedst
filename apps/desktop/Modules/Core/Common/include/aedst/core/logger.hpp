#pragma once

#include <spdlog/spdlog.h>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace aedst::core {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Critical,
    Off,
};

/// Text: `[time] [level] [logger] [t:thread] [cid:id] message`, one line each.
/// Json: one JSON object per line (JSON Lines) with ts (UTC, ISO 8601, ms), level,
///       logger, thread, cid (only inside a CorrelationScope) and msg - for log
///       shippers; the message is escaped so a newline in it cannot forge a record.
enum class LogFormat {
    Text,
    Json,
};

struct LoggerConfig {
    std::string name{"aedst"};
    LogLevel level{LogLevel::Info};
    LogFormat format{LogFormat::Text};
    /// Mask secrets (passwords, tokens, cookies, Bearer credentials) and e-mail
    /// addresses in every message before any sink sees it; see redactSecrets().
    /// Only turn off in a throwaway local debugging session.
    bool redact{true};
    bool console{true};
    /// Empty disables the rotating file sink.
    std::filesystem::path logFile;
    std::size_t maxFileSizeBytes{5U * 1024U * 1024U};
    std::size_t maxFiles{3};
};

/// The application's own logger (spdlog: console + rotating file).
///
/// Two logging systems coexist on purpose:
///  - AedstLogger (this) is for AEDST code — use cases, repositories, network
///    and persistence. It is Qt-free, so Domain/Data code and GoogleTest
///    binaries use it without a QCoreApplication, and its rotating file is
///    what support asks users to attach.
///  - Qt's qDebug/qWarning/qCritical + QLoggingCategory stay reserved for
///    Qt/QML framework messages (QML binding warnings, plugin loading, qt.*
///    categories). They are not forwarded here, so turning on a noisy qt.*
///    category never floods the product log, and QT_LOGGING_RULES keeps
///    working exactly as Qt documents it.
/// Every record carries the calling thread's correlation id (`[cid:...]`, see
/// CorrelationScope in correlation.hpp; `-` outside a scope).
///
/// AEDST code should never call qDebug(); Qt code paths never call AedstLogger.
class AedstLogger final {
public:
    AedstLogger() = delete;

    /// Replaces the process-wide logger. `extraSinks` is how tests capture output.
    static void init(const LoggerConfig& config, std::vector<spdlog::sink_ptr> extraSinks = {});
    static void setLevel(LogLevel level);
    static void flush();
    static void shutdown();

    /// Lazily falls back to a console-only logger if init() was never called
    /// (unit tests, early startup).
    [[nodiscard]] static std::shared_ptr<spdlog::logger> get();

    template <typename... Args>
    static void trace(spdlog::format_string_t<Args...> format, Args&&... args) {
        get()->trace(format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void debug(spdlog::format_string_t<Args...> format, Args&&... args) {
        get()->debug(format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void info(spdlog::format_string_t<Args...> format, Args&&... args) {
        get()->info(format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void warn(spdlog::format_string_t<Args...> format, Args&&... args) {
        get()->warn(format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void error(spdlog::format_string_t<Args...> format, Args&&... args) {
        get()->error(format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void critical(spdlog::format_string_t<Args...> format, Args&&... args) {
        get()->critical(format, std::forward<Args>(args)...);
    }
};

} // namespace aedst::core
