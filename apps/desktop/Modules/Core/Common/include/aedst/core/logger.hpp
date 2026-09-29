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

struct LoggerConfig {
    std::string name{"aedst"};
    LogLevel level{LogLevel::Info};
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
