#include "aedst/analytics/no_op_analytics_tracker.hpp"

#include "aedst/core/logger.hpp"

#include <string>

namespace aedst::analytics {

using core::AedstLogger;

void NoOpAnalyticsTracker::track(std::string_view event, const AnalyticsProperties& properties) {
    std::string rendered;
    for (const auto& [key, value] : properties) {
        if (!rendered.empty()) {
            rendered += ", ";
        }
        rendered += key;
        rendered += '=';
        rendered += value;
    }
    AedstLogger::debug("analytics: {} {{{}}}", event, rendered);
}

void NoOpAnalyticsTracker::identify(std::string_view userId) {
    AedstLogger::debug("analytics: identify {}", userId);
}

void NoOpAnalyticsTracker::reset() {
    AedstLogger::debug("analytics: reset");
}

} // namespace aedst::analytics
