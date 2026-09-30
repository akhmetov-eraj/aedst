#include "dbus_notification_service.hpp"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

#include <string_view>

namespace aedst::platform {
namespace {

constexpr qsizetype kMaxTitleChars = 256;
constexpr qsizetype kMaxBodyChars = 2048;

QString serviceName() {
    return QStringLiteral("org.freedesktop.Notifications");
}

bool isServerRunning() {
    const QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return false;
    }
    QDBusConnectionInterface* busInterface = bus.interface();
    if (busInterface == nullptr) {
        return false;
    }
    return busInterface->isServiceRegistered(serviceName()).value();
}

/// Notification spec urgency levels: 0 low, 1 normal, 2 critical.
uchar urgencyFor(NotificationSeverity severity) {
    return severity == NotificationSeverity::Error ? uchar{2} : uchar{1};
}

QString iconFor(NotificationSeverity severity) {
    switch (severity) {
    case NotificationSeverity::Warning:
        return QStringLiteral("dialog-warning");
    case NotificationSeverity::Error:
        return QStringLiteral("dialog-error");
    case NotificationSeverity::Info:
        break;
    }
    return QStringLiteral("dialog-information");
}

} // namespace

DBusNotificationService::DBusNotificationService(NotificationService& fallback)
    : fallback_(fallback), available_(isServerRunning()) {}

bool DBusNotificationService::isAvailable() const {
    return available_;
}

void DBusNotificationService::show(std::string_view title, std::string_view body, NotificationSeverity severity) {
    if (!available_) {
        fallback_.show(title, body, severity);
        return;
    }
    // Summary is plain text; the body may be interpreted as markup by the
    // server, so escape it: server-provided text must never inject markup.
    const QString summary =
        QString::fromUtf8(title.data(), static_cast<qsizetype>(title.size())).left(kMaxTitleChars);
    const QString text =
        QString::fromUtf8(body.data(), static_cast<qsizetype>(body.size())).left(kMaxBodyChars).toHtmlEscaped();

    QVariantMap hints;
    hints.insert(QStringLiteral("urgency"), QVariant::fromValue(urgencyFor(severity)));

    QDBusMessage message = QDBusMessage::createMethodCall(
        serviceName(), QStringLiteral("/org/freedesktop/Notifications"), serviceName(), QStringLiteral("Notify"));
    message << QCoreApplication::applicationName()                  // app_name
            << QVariant::fromValue(quint32{0})                       // replaces_id
            << iconFor(severity)                                     // app_icon
            << summary                                               // summary
            << text                                                  // body
            << QStringList{}                                         // actions
            << hints                                                 // hints
            << -1;                                                   // expire_timeout: server default

    const QDBusPendingCall pending = QDBusConnection::sessionBus().asyncCall(message);
    if (pending.isError()) {
        fallback_.show(title, body, severity);
    }
}

} // namespace aedst::platform
