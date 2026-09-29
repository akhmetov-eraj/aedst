#include "aedst/core/logger.hpp"

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <mutex>

namespace aedst::core {
namespace {

constexpr auto kPattern = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] [t:%t] %v";

std::mutex& loggerMutex() {
    static std::mutex mutex;
    return mutex;
}

std::shared_ptr<spdlog::logger>& loggerInstance() {
    static std::shared_ptr<spdlog::logger> instance;
    return instance;
}

spdlog::level::level_enum toSpdlog(LogLevel level) {
    switch (level) {
    case LogLevel::Trace:
        return spdlog::level::trace;
    case LogLevel::Debug:
        return spdlog::level::debug;
    case LogLevel::Info:
        return spdlog::level::info;
    case LogLevel::Warn:
        return spdlog::level::warn;
    case LogLevel::Error:
        return spdlog::level::err;
    case LogLevel::Critical:
        return spdlog::level::critical;
    case LogLevel::Off:
        return spdlog::level::off;
    }
    return spdlog::level::info;
}

std::shared_ptr<spdlog::logger> makeFallbackLogger() {
    auto logger = std::make_shared<spdlog::logger>("aedst", std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    logger->set_pattern(kPattern);
    return logger;
}

} // namespace

void AedstLogger::init(const LoggerConfig& config, std::vector<spdlog::sink_ptr> extraSinks) {
    std::vector<spdlog::sink_ptr> sinks;
    if (config.console) {
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    }
    if (!config.logFile.empty()) {
        if (config.logFile.has_parent_path()) {
            std::error_code ignored;
            std::filesystem::create_directories(config.logFile.parent_path(), ignored);
        }
        sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            config.logFile.string(), config.maxFileSizeBytes, config.maxFiles));
    }
    for (auto& sink : extraSinks) {
        sinks.push_back(std::move(sink));
    }

    auto logger = std::make_shared<spdlog::logger>(config.name, sinks.begin(), sinks.end());
    logger->set_level(toSpdlog(config.level));
    logger->set_pattern(kPattern);
    logger->flush_on(spdlog::level::warn);

    const std::scoped_lock lock(loggerMutex());
    loggerInstance() = std::move(logger);
}

void AedstLogger::setLevel(LogLevel level) {
    get()->set_level(toSpdlog(level));
}

void AedstLogger::flush() {
    get()->flush();
}

void AedstLogger::shutdown() {
    const std::scoped_lock lock(loggerMutex());
    if (auto& instance = loggerInstance()) {
        instance->flush();
        instance.reset();
    }
}

std::shared_ptr<spdlog::logger> AedstLogger::get() {
    const std::scoped_lock lock(loggerMutex());
    auto& instance = loggerInstance();
    if (!instance) {
        instance = makeFallbackLogger();
    }
    return instance;
}

} // namespace aedst::core
