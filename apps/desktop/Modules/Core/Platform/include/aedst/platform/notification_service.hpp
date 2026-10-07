#pragma once

#include <QObject>
#include <QString>

#include <memory>
#include <string_view>

namespace aedst::platform {

enum class NotificationSeverity {
    Info,
    Warning,
    Error,
};

[[nodiscard]] constexpr std::string_view toString(NotificationSeverity severity) noexcept {
    switch (severity) {
    case NotificationSeverity::Warning:
        return "warning";
    case NotificationSeverity::Error:
        return "error";
    case NotificationSeverity::Info:
        break;
    }
    return "info";
}

/// Shows a user-visible notification. Text is plain UTF-8; implementations
/// truncate and escape it, so callers can pass server-provided strings.
class NotificationService {
public:
    NotificationService() = default;
    NotificationService(const NotificationService&) = delete;
    NotificationService& operator=(const NotificationService&) = delete;
    NotificationService(NotificationService&&) = delete;
    NotificationService& operator=(NotificationService&&) = delete;
    virtual ~NotificationService() = default;

    /// Runtime capability check (is there anything that can display this?).
    [[nodiscard]] virtual bool isAvailable() const = 0;
    virtual void show(std::string_view title, std::string_view body, NotificationSeverity severity) = 0;
};

/// Universal fallback: does not display anything itself, it emits
/// notificationRaised() so QML can show an in-app AedstToast. `severity` is
/// "info" | "warning" | "error". isAvailable() is true once something (the QML
/// layer) is connected to the signal.
class InAppNotificationService final : public QObject, public NotificationService {
    Q_OBJECT

public:
    explicit InAppNotificationService(QObject* parent = nullptr);

    [[nodiscard]] bool isAvailable() const override;
    void show(std::string_view title, std::string_view body, NotificationSeverity severity) override;

signals:
    void notificationRaised(const QString& title, const QString& body, const QString& severity);
};

/// Chooses the best available service:
///  - Linux with the optional Qt6::DBus and a running org.freedesktop.Notifications
///    server: a native desktop notification (falling back to `inAppFallback` if
///    the call cannot be sent);
///  - otherwise `nativeDesktop` when given and available - the app passes its
///    TrayNotificationService (QSystemTrayIcon::showMessage: Windows toast/balloon, Linux tray), see tray_notification_service.hpp;
///  - otherwise forwards to `inAppFallback` (AedstToast).
/// `inAppFallback` and `nativeDesktop` must outlive the returned service.
///
/// Still not implemented: WinRT ToastNotificationManager (actions, history;
/// needs an AppUserModelID shortcut). The tray path covers "show a native notification"
/// without it.
[[nodiscard]] std::unique_ptr<NotificationService> makeNotificationService(NotificationService& inAppFallback,
                                                                           NotificationService* nativeDesktop = nullptr);

} // namespace aedst::platform
