#include "Navigation/media_destinations.hpp"

#include "aedst/navigation/nav_graph.hpp"

#include <QString>
#include <QUrl>

namespace aedst::feature::media {
namespace {

[[nodiscard]] QUrl toUrl(std::string_view url) {
    return QUrl(QString::fromUtf8(url.data(), static_cast<qsizetype>(url.size())));
}

} // namespace

void registerMediaDestinations(navigation::NavGraph& graph) {
    graph.registerDestination(navigation::routes::kMedia, toUrl(kMediaLibraryViewUrl));
    graph.registerDestination(navigation::routes::kMediaViewer, toUrl(kMediaViewerViewUrl));
}

} // namespace aedst::feature::media
