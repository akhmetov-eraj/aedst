#pragma once

#include <functional>
#include <map>
#include <string>
#include <string_view>

namespace aedst::analytics {

using AnalyticsProperties = std::map<std::string, std::string, std::less<>>;

/// Event names live in Core, not in each feature, so they stay one flat,
/// greppable vocabulary for whatever backend eventually receives them.
namespace events {

inline constexpr std::string_view kLoginSucceeded = "auth_login_succeeded";
inline constexpr std::string_view kLoginFailed = "auth_login_failed";
inline constexpr std::string_view kLogout = "auth_logout";

} // namespace events

/// Vendor-neutral analytics port. Features depend on this interface only; the
/// concrete backend is chosen once in App/AppDependencies.
class AnalyticsTracker {
public:
    AnalyticsTracker() = default;
    AnalyticsTracker(const AnalyticsTracker&) = delete;
    AnalyticsTracker& operator=(const AnalyticsTracker&) = delete;
    AnalyticsTracker(AnalyticsTracker&&) = delete;
    AnalyticsTracker& operator=(AnalyticsTracker&&) = delete;
    virtual ~AnalyticsTracker() = default;

    virtual void track(std::string_view event, const AnalyticsProperties& properties) = 0;
    virtual void identify(std::string_view userId) = 0;
    virtual void reset() = 0;
};

} // namespace aedst::analytics
