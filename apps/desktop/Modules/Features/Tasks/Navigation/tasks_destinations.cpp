#include "Navigation/tasks_destinations.hpp"

#include "Navigation/tasks_route.hpp"
#include "aedst/navigation/nav_graph.hpp"

#include <QString>
#include <QUrl>

namespace aedst::feature::tasks {
namespace {

[[nodiscard]] QUrl toUrl(std::string_view url) {
    return QUrl(QString::fromUtf8(url.data(), static_cast<qsizetype>(url.size())));
}

} // namespace

void registerTasksDestinations(navigation::NavGraph& graph) {
    graph.registerDestination(navigation::routes::kTasks, toUrl(kTaskListViewUrl));
    graph.registerDestination(navigation::routes::kTaskDetail, toUrl(kTaskDetailViewUrl));
}

} // namespace aedst::feature::tasks
