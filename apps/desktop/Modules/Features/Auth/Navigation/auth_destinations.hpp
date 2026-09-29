#pragma once

namespace aedst::navigation {
class NavGraph;
} // namespace aedst::navigation

namespace aedst::feature::auth {

/// Adds this feature's screens to the app-wide graph. Called only by App/.
void registerAuthDestinations(navigation::NavGraph& graph);

} // namespace aedst::feature::auth
