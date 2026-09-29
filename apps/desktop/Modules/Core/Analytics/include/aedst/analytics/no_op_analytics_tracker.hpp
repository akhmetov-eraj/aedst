#pragma once

#include "aedst/analytics/analytics_tracker.hpp"

namespace aedst::analytics {

/// Default until a real analytics backend is chosen: records events at debug
/// level in the AedstLogger file so they're still inspectable locally.
class NoOpAnalyticsTracker final : public AnalyticsTracker {
public:
    void track(std::string_view event, const AnalyticsProperties& properties) override;
    void identify(std::string_view userId) override;
    void reset() override;
};

} // namespace aedst::analytics
