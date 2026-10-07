#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aedst::feature::notifications {

enum class NotificationKind {
    Unknown,
    TaskCompleted,
    TaskDueSoon,
    MediaReady,
    System,
};

[[nodiscard]] constexpr std::string_view toString(NotificationKind kind) noexcept {
    switch (kind) {
    case NotificationKind::TaskCompleted:
        return "task_completed";
    case NotificationKind::TaskDueSoon:
        return "task_due_soon";
    case NotificationKind::MediaReady:
        return "media_ready";
    case NotificationKind::System:
        return "system";
    case NotificationKind::Unknown:
        break;
    }
    return "unknown";
}

struct Notification {
    std::string id;
    NotificationKind kind{NotificationKind::Unknown};
    std::string title;
    std::string body;
    /// Relative in-app route ("/tasks/<id>"); opened via Router::openLink.
    std::string link;
    std::string subjectId;
    std::chrono::system_clock::time_point createdAt;
    std::optional<std::chrono::system_clock::time_point> readAt;

    [[nodiscard]] bool isRead() const noexcept { return readAt.has_value(); }

    friend bool operator==(const Notification&, const Notification&) = default;
};

struct NotificationPage {
    std::vector<Notification> notifications;
    std::string nextPageToken;
};

/// aedst.notifications.v1.Preferences. Desktop has no push channel (no FCM /
/// APNs / Web Push registration), so it reads `pushEnabled` as "show native
/// desktop notifications" and the per-kind switches as filters for them; the
/// in-app inbox always shows everything.
struct NotificationPreferences {
    bool pushEnabled{true};
    bool taskCompleted{true};
    bool taskDueSoon{true};
    bool mediaReady{true};

    [[nodiscard]] bool allows(NotificationKind kind) const noexcept {
        if (!pushEnabled) {
            return false;
        }
        switch (kind) {
        case NotificationKind::TaskCompleted:
            return taskCompleted;
        case NotificationKind::TaskDueSoon:
            return taskDueSoon;
        case NotificationKind::MediaReady:
            return mediaReady;
        case NotificationKind::System:
        case NotificationKind::Unknown:
            break;
        }
        return true; // system broadcasts are not user-filterable
    }

    friend bool operator==(const NotificationPreferences&, const NotificationPreferences&) = default;
};

} // namespace aedst::feature::notifications
