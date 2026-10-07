#pragma once

#include "Domain/new_notification_detector.hpp"
#include "Presentation/notification_list_model.hpp"

#include <QObject>
#include <QString>
#include <QTimer>

#include <memory>
#include <string>

namespace aedst::analytics {
class AnalyticsTracker;
} // namespace aedst::analytics

namespace aedst::core {
class ErrorSink;
} // namespace aedst::core

namespace aedst::navigation {
class Router;
} // namespace aedst::navigation

namespace aedst::platform {
class NotificationService;
} // namespace aedst::platform

namespace aedst::feature::notifications {

class NotificationRepository;

/// Inbox, unread badge, mark read, preferences, and native desktop notifications.
///
/// Delivery model on desktop: there is no FCM/APNs/Web Push channel for a desktop
/// app, so the client POLLS GetUnreadCount (kPollIntervalMs while the window is
/// active, kBackgroundPollIntervalMs otherwise; exponential backoff up to
/// kMaxBackoffMs on retryable errors). When the count rises it fetches the newest
/// unread notifications and NewNotificationDetector picks what to show through
/// platform::NotificationService (QSystemTrayIcon toast / D-Bus / in-app fallback),
/// honouring the server-side preferences. Polling runs only between start() (signed
/// in) and stop() (signed out). A server-streaming "watch unread" RPC would replace
/// the poll without UI changes; the contract has none today.
class NotificationsViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int unreadCount READ unreadCount NOTIFY unreadCountChanged)
    Q_PROPERTY(QAbstractItemModel* inbox READ inbox CONSTANT)
    Q_PROPERTY(bool unreadOnly READ unreadOnly WRITE setUnreadOnly NOTIFY unreadOnlyChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY hasMoreChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    // Preferences (aedst.notifications.v1.Preferences).
    Q_PROPERTY(bool preferencesLoaded READ preferencesLoaded NOTIFY preferencesChanged)
    Q_PROPERTY(bool desktopNotifications READ desktopNotifications NOTIFY preferencesChanged)
    Q_PROPERTY(bool taskCompleted READ taskCompleted NOTIFY preferencesChanged)
    Q_PROPERTY(bool taskDueSoon READ taskDueSoon NOTIFY preferencesChanged)
    Q_PROPERTY(bool mediaReady READ mediaReady NOTIFY preferencesChanged)
    Q_PROPERTY(bool isSavingPreferences READ isSavingPreferences NOTIFY isSavingPreferencesChanged)

public:
    static constexpr int kPollIntervalMs = 30'000;
    static constexpr int kBackgroundPollIntervalMs = 120'000;
    static constexpr int kMaxBackoffMs = 300'000;

    NotificationsViewModel(std::shared_ptr<NotificationRepository> repository,
                           std::shared_ptr<core::ErrorSink> errors,
                           std::shared_ptr<analytics::AnalyticsTracker> analytics,
                           platform::NotificationService* desktopNotifier, navigation::Router* router,
                           QObject* parent = nullptr);

    [[nodiscard]] int unreadCount() const noexcept { return unreadCount_; }
    [[nodiscard]] QAbstractItemModel* inbox() noexcept { return &inbox_; }
    [[nodiscard]] NotificationListModel& inboxModel() noexcept { return inbox_; }
    [[nodiscard]] bool unreadOnly() const noexcept { return unreadOnly_; }
    void setUnreadOnly(bool unreadOnly);
    [[nodiscard]] bool isLoading() const noexcept { return isLoading_; }
    [[nodiscard]] bool hasMore() const { return !nextPageToken_.empty(); }
    [[nodiscard]] QString errorMessage() const { return errorMessage_; }
    [[nodiscard]] bool preferencesLoaded() const noexcept { return preferencesLoaded_; }
    [[nodiscard]] bool desktopNotifications() const noexcept { return preferences_.pushEnabled; }
    [[nodiscard]] bool taskCompleted() const noexcept { return preferences_.taskCompleted; }
    [[nodiscard]] bool taskDueSoon() const noexcept { return preferences_.taskDueSoon; }
    [[nodiscard]] bool mediaReady() const noexcept { return preferences_.mediaReady; }
    [[nodiscard]] bool isSavingPreferences() const noexcept { return isSavingPreferences_; }

    /// Signed in: reset the detector to "now", load preferences, poll immediately.
    Q_INVOKABLE void start();
    /// Signed out: stop polling and forget everything user-specific.
    Q_INVOKABLE void stop();
    /// Window became active/inactive: poll faster while the user looks at the app.
    Q_INVOKABLE void setForeground(bool foreground);
    Q_INVOKABLE void pollNow();

    Q_INVOKABLE void reload();
    Q_INVOKABLE void loadMore();
    Q_INVOKABLE void markRead(const QString& notificationId);
    Q_INVOKABLE void markAllRead();
    /// Marks the row read and opens its link (allow-listed by Router::openLink).
    Q_INVOKABLE void open(int row);
    Q_INVOKABLE void openPreferences();
    Q_INVOKABLE void loadPreferences();
    Q_INVOKABLE void savePreferences(bool desktopNotifications, bool taskCompleted, bool taskDueSoon,
                                     bool mediaReady);

signals:
    void unreadCountChanged();
    void unreadOnlyChanged();
    void isLoadingChanged();
    void hasMoreChanged();
    void errorMessageChanged();
    void preferencesChanged();
    void isSavingPreferencesChanged();

private:
    void onUnreadCount(int count);
    void announceNew();
    void scheduleNextPoll(bool failed);
    void setUnreadCount(int count);
    void setLoading(bool loading);
    void setErrorMessage(const QString& message);
    void setNextPageToken(std::string token);

    std::shared_ptr<NotificationRepository> repository_;
    std::shared_ptr<core::ErrorSink> errors_;
    std::shared_ptr<analytics::AnalyticsTracker> analytics_;
    platform::NotificationService* desktopNotifier_;
    navigation::Router* router_;

    NotificationListModel inbox_;
    NewNotificationDetector detector_;
    NotificationPreferences preferences_;
    QTimer pollTimer_;
    QString errorMessage_;
    std::string nextPageToken_;
    int unreadCount_{0};
    int failedPolls_{0};
    bool running_{false};
    bool foreground_{true};
    bool unreadOnly_{false};
    bool isLoading_{false};
    bool preferencesLoaded_{false};
    bool isSavingPreferences_{false};
    bool pollInFlight_{false};
    quint64 session_{0};
};

} // namespace aedst::feature::notifications
