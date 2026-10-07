#pragma once

#include "aedst/navigation/route.hpp"

#include <string>
#include <string_view>

namespace aedst::feature::tasks {

/// qrc paths produced by qt_add_qml_module(AedstFeatureTasksViews URI
/// Aedst.Feature.Tasks.Views RESOURCE_PREFIX /qt/qml) with each file aliased to its name.
inline constexpr std::string_view kTaskListViewUrl = "qrc:/qt/qml/Aedst/Feature/Tasks/Views/TaskListView.qml";
inline constexpr std::string_view kTaskDetailViewUrl = "qrc:/qt/qml/Aedst/Feature/Tasks/Views/TaskDetailView.qml";

[[nodiscard]] inline navigation::Route taskListRoute() {
    return navigation::Route{.name = std::string(navigation::routes::kTasks), .params = {}};
}

[[nodiscard]] inline navigation::Route taskDetailRoute(std::string taskId) {
    return navigation::Route{.name = std::string(navigation::routes::kTaskDetail),
                             .params = {{std::string(navigation::params::kTaskId), std::move(taskId)}}};
}

} // namespace aedst::feature::tasks
