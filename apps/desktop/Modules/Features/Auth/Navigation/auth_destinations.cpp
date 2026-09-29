#include "Navigation/auth_destinations.hpp"

#include "Navigation/auth_route.hpp"
#include "aedst/navigation/nav_graph.hpp"

#include <QString>
#include <QUrl>

namespace aedst::feature::auth {

void registerAuthDestinations(navigation::NavGraph& graph) {
    graph.registerDestination(navigation::routes::kLogin,
                              QUrl(QString::fromUtf8(kLoginViewUrl.data(), static_cast<qsizetype>(kLoginViewUrl.size()))));
}

} // namespace aedst::feature::auth
