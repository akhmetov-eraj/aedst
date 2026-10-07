#include "Navigation/search_destinations.hpp"

#include "aedst/navigation/nav_graph.hpp"

#include <QString>
#include <QUrl>

namespace aedst::feature::search {

void registerSearchDestinations(navigation::NavGraph& graph) {
    graph.registerDestination(navigation::routes::kSearch,
                              QUrl(QString::fromUtf8(kSearchViewUrl.data(),
                                                     static_cast<qsizetype>(kSearchViewUrl.size()))));
}

} // namespace aedst::feature::search
