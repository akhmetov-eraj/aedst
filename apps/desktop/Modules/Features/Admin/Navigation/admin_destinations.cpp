#include "Navigation/admin_destinations.hpp"

#include "aedst/navigation/nav_graph.hpp"

#include <QString>
#include <QUrl>

namespace aedst::feature::admin {

void registerAdminDestinations(navigation::NavGraph& graph) {
    graph.registerDestination(navigation::routes::kAdmin,
                              QUrl(QString::fromUtf8(kAdminViewUrl.data(), static_cast<qsizetype>(kAdminViewUrl.size()))));
}

} // namespace aedst::feature::admin
