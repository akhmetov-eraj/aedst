#pragma once

#include "aedst/core/redaction.hpp"

#include <spdlog/details/log_msg.h>
#include <spdlog/formatter.h>
#include <spdlog/sinks/sink.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace aedst::core {

/// Redacts a record's payload ONCE and hands it to every wrapped sink (console, file, test sinks).
/// A redacting formatter per sink would run the scrubber once per sink for the same message. The wrapped sinks
/// keep their own level and formatter; this sink only forwards.
class RedactingSink final : public spdlog::sinks::sink {
public:
    RedactingSink(std::vector<spdlog::sink_ptr> inner, bool redact) : inner_(std::move(inner)), redact_(redact) {}

    void log(const spdlog::details::log_msg& msg) override {
        if (redact_) {
            const std::string_view original(msg.payload.data(), msg.payload.size());
            const std::string redacted = redactSecrets(original);
            if (redacted != original) {
                spdlog::details::log_msg copy(msg);
                copy.payload = spdlog::string_view_t(redacted.data(), redacted.size());
                forward(copy);
                return;
            }
        }
        forward(msg);
    }

    void flush() override {
        for (const auto& sink : inner_) {
            sink->flush();
        }
    }

    void set_pattern(const std::string& pattern) override {
        for (const auto& sink : inner_) {
            sink->set_pattern(pattern);
        }
    }

    void set_formatter(std::unique_ptr<spdlog::formatter> formatter) override {
        for (const auto& sink : inner_) {
            sink->set_formatter(formatter->clone());
        }
    }

private:
    void forward(const spdlog::details::log_msg& msg) const {
        for (const auto& sink : inner_) {
            if (sink->should_log(msg.level)) {
                sink->log(msg);
            }
        }
    }

    std::vector<spdlog::sink_ptr> inner_;
    bool redact_;
};

} // namespace aedst::core
