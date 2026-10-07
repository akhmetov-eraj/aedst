#pragma once

namespace aedst::navigation {
class NavGraph;
} // namespace aedst::navigation

namespace aedst::feature::tasks {

/// Adds this feature's screens to the app-wide graph. Called only by App/.
void registerTasksDestinations(navigation::NavGraph& graph);

} // namespace aedst::feature::tasks
