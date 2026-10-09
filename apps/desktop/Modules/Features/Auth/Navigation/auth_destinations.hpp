#pragma once

#include "aedst/plugin/feature_plugin.hpp"

#include <memory>

namespace aedst::feature::auth {

/// This feature as a plugin (identity + screens); the composition root adds it to the PluginManager and
/// registers the routes of the active plugins in the NavGraph. See ADR-0033.
[[nodiscard]] std::unique_ptr<plugin::IFeaturePlugin> makeAuthPlugin();

} // namespace aedst::feature::auth
