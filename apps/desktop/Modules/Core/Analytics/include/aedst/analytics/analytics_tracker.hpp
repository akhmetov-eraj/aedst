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

inline constexpr std::string_view kLoginSucceeded = "login_success";
inline constexpr std::string_view kLoginFailed = "login_failure";
inline constexpr std::string_view kLogout = "logout";
inline constexpr std::string_view kTaskCreated = "task_created";
inline constexpr std::string_view kTaskCompleted = "task_completed";
inline constexpr std::string_view kSearchPerformed = "search_performed";
inline constexpr std::string_view kSearchResultOpened = "search_result_opened";
inline constexpr std::string_view kNotificationOpened = "notification_opened";
inline constexpr std::string_view kMediaUploaded = "media_uploaded";
inline constexpr std::string_view kAdminBroadcastSent = "admin_broadcast_sent";

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
