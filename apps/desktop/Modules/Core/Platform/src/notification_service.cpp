#include "aedst/platform/notification_service.hpp"

#include <QMetaMethod>

#include <memory>
#include <string_view>

#if defined(AEDST_PLATFORM_HAS_DBUS)
#include "dbus_notification_service.hpp"
#endif

namespace aedst::platform {
namespace {

constexpr qsizetype kMaxTitleChars = 256;
constexpr qsizetype kMaxBodyChars = 2048;

class ForwardingNotificationService final : public NotificationService {
public:
    explicit ForwardingNotificationService(NotificationService& target) : target_(target) {}

    [[nodiscard]] bool isAvailable() const override { return target_.isAvailable(); }

    void show(std::string_view title, std::string_view body, NotificationSeverity severity) override {
        target_.show(title, body, severity);
    }

private:
    NotificationService& target_;
};

} // namespace

InAppNotificationService::InAppNotificationService(QObject* parent) : QObject(parent) {}

bool InAppNotificationService::isAvailable() const {
    return isSignalConnected(QMetaMethod::fromSignal(&InAppNotificationService::notificationRaised));
}

void InAppNotificationService::show(std::string_view title, std::string_view body, NotificationSeverity severity) {
    const QString qTitle =
        QString::fromUtf8(title.data(), static_cast<qsizetype>(title.size())).left(kMaxTitleChars);
    const QString qBody = QString::fromUtf8(body.data(), static_cast<qsizetype>(body.size())).left(kMaxBodyChars);
    const std::string_view severityName = toString(severity);
    emit notificationRaised(qTitle, qBody,
                            QString::fromUtf8(severityName.data(), static_cast<qsizetype>(severityName.size())));
}

std::unique_ptr<NotificationService> makeNotificationService(NotificationService& inAppFallback) {
#if defined(AEDST_PLATFORM_HAS_DBUS)
    auto native = std::make_unique<DBusNotificationService>(inAppFallback);
    if (native->isAvailable()) {
        return native;
    }
#endif
    return std::make_unique<ForwardingNotificationService>(inAppFallback);
}

} // namespace aedst::platform
