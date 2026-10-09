#pragma once

#include "aedst/core/outcome.hpp"
#include "aedst/plugin/capability_set.hpp"
#include "aedst/plugin/feature_plugin.hpp"
#include "aedst/plugin/service_registry.hpp"

#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace aedst::plugin {

enum class PluginState {
    /// Added, not evaluated yet, or waiting for capabilities that are not there (yet).
    Pending,
    /// Passed validation and gating; its services and routes are in effect.
    Active,
    /// Permanently refused for this process (invalid descriptor, API mismatch, route collision,
    /// registration failure). Re-running activate() never changes it.
    Rejected,
};

struct PluginStatus {
    std::string id;
    std::string version;
    PluginOrigin origin{PluginOrigin::BuiltIn};
    PluginState state{PluginState::Pending};
    /// Human-readable reason for Pending/Rejected, empty when Active. Diagnostics only (logs).
    std::string reason;
};

/// Owns the plugins and decides which are active.
///
/// Rules, in order, per plugin:
///  1. descriptor: valid id, semver version, apiVersion == kPluginApiVersion, unique id;
///  2. routes: names non-empty and not already claimed by an earlier plugin (a runtime plugin can
///     never shadow a built-in one because built-ins are added first), views are `qrc:/` URLs;
///  3. gating: all requiredCapabilities and the featureFlag. With an UNKNOWN CapabilitySet a
///     built-in plugin is activated (first-party features keep working offline and against servers
///     that predate the capabilities service; the server still authorises every RPC), a runtime
///     plugin stays Pending (fail closed);
///  4. registerServices(): on failure or exception the registry is rolled back and the plugin is
///     Rejected.
/// Plugins are evaluated in the order they were added. There is no deactivation: capabilities that
/// disappear later take effect after a restart.
///
/// Not thread-safe; GUI thread only.
class PluginManager final {
public:
    PluginManager() = default;
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;
    PluginManager(PluginManager&&) = delete;
    PluginManager& operator=(PluginManager&&) = delete;
    ~PluginManager() = default;

    /// Takes ownership. An error means the plugin was dropped (null, or an id that is already
    /// taken). Everything else is judged in activate() so one bad plugin shows up in report().
    [[nodiscard]] core::VoidOutcome add(std::unique_ptr<IFeaturePlugin> plugin, PluginOrigin origin);

    /// Evaluates every plugin that is not Active or Rejected yet. Safe to call again when the
    /// capabilities change; already Active plugins are left alone.
    void activate(const ICapabilitySource& capabilities, ServiceRegistry& registry);

    [[nodiscard]] bool isActive(std::string_view id) const;
    [[nodiscard]] std::vector<PluginStatus> report() const;
    /// Routes of the Active plugins, in activation order.
    [[nodiscard]] std::vector<RouteContribution> activeRoutes() const;

private:
    struct Entry {
        std::unique_ptr<IFeaturePlugin> plugin;
        PluginOrigin origin{PluginOrigin::BuiltIn};
        PluginState state{PluginState::Pending};
        std::string reason;
        std::vector<RouteContribution> routes; // filled when Active
    };

    void evaluate(Entry& entry, const CapabilitySet& capabilities, ServiceRegistry& registry);
    static void reject(Entry& entry, std::string reason);

    std::vector<Entry> entries_;
    std::set<std::string, std::less<>> ids_;
    std::set<std::string, std::less<>> claimedRoutes_;
};

/// `[a-z][a-z0-9._-]{2,63}`. Exposed for the loader and the tests.
[[nodiscard]] bool isValidPluginId(std::string_view id) noexcept;

} // namespace aedst::plugin
