#include "Navigation/notifications_destinations.hpp"

#include "aedst/navigation/nav_graph.hpp"

#include <QString>
#include <QUrl>

namespace aedst::feature::notifications {
namespace {

[[nodiscard]] QUrl toUrl(std::string_view url) {
    return QUrl(QString::fromUtf8(url.data(), static_cast<qsizetype>(url.size())));
}

} // namespace

void registerNotificationsDestinations(navigation::NavGraph& graph) {
    graph.registerDestination(navigation::routes::kNotifications, toUrl(kNotificationsViewUrl));
    graph.registerDestination(navigation::routes::kNotificationPreferences, toUrl(kNotificationPreferencesViewUrl));
}

} // namespace aedst::feature::notifications
