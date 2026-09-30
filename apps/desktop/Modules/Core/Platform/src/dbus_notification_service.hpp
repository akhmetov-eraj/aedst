#pragma once

// Private to AedstPlatform; compiled only when Qt6::DBus was found on a
// non-Apple Unix (AEDST_PLATFORM_HAS_DBUS).

#include "aedst/platform/notification_service.hpp"

#include <string_view>

namespace aedst::platform {

/// org.freedesktop.Notifications.Notify over the session bus. Delivery is
/// fire-and-forget: a server-side error after the call was accepted is not
/// observed (the in-app fallback is used only when the call cannot be sent).
class DBusNotificationService final : public NotificationService {
public:
    explicit DBusNotificationService(NotificationService& fallback);

    [[nodiscard]] bool isAvailable() const override;
    void show(std::string_view title, std::string_view body, NotificationSeverity severity) override;

private:
    NotificationService& fallback_;
    bool available_;
};

} // namespace aedst::platform
