#include "aedst/plugin/plugin_manager.hpp"

#include "aedst/core/logger.hpp"
#include "aedst/update/semver.hpp"

#include <algorithm>
#include <exception>
#include <utility>

namespace aedst::plugin {
namespace {

constexpr std::size_t kMinIdChars = 3;
constexpr std::size_t kMaxIdChars = 64;

[[nodiscard]] bool isIdChar(char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
}

[[nodiscard]] bool isQrcUrl(std::string_view url) noexcept {
    constexpr std::string_view kPrefix = "qrc:/";
    return url.size() > kPrefix.size() && url.substr(0, kPrefix.size()) == kPrefix;
}

/// A link prefix is one lower-case ASCII word: it becomes an `aedst://` host and a path segment, nothing else.
[[nodiscard]] bool isLinkPrefix(std::string_view prefix) noexcept {
    return !prefix.empty() && prefix.size() <= 32 &&
           std::ranges::all_of(prefix, [](char c) { return c >= 'a' && c <= 'z'; });
}

/// Empty when the descriptor is acceptable, else the reason it is not.
[[nodiscard]] std::string descriptorProblem(const PluginDescriptor& descriptor) {
    if (!isValidPluginId(descriptor.id)) {
        return "invalid plugin id";
    }
    if (descriptor.apiVersion != kPluginApiVersion) {
        return "plugin API version " + std::to_string(descriptor.apiVersion) + " is not supported (host speaks " +
               std::to_string(kPluginApiVersion) + ")";
    }
    if (!update::parseSemVer(descriptor.version)) {
        return "version is not a semantic version";
    }
    return {};
}

} // namespace

bool isValidPluginId(std::string_view id) noexcept {
    if (id.size() < kMinIdChars || id.size() > kMaxIdChars) {
        return false;
    }
    if (id.front() < 'a' || id.front() > 'z') {
        return false;
    }
    return std::ranges::all_of(id, isIdChar);
}

core::VoidOutcome PluginManager::add(std::unique_ptr<IFeaturePlugin> plugin, PluginOrigin origin) {
    if (!plugin) {
        return core::fail(core::Error::invalidInput("Cannot add a null plugin"));
    }
    const std::string id = plugin->descriptor().id;
    if (!ids_.insert(id).second) {
        // Not even listed in report(): a second plugin must not be able to mask the first one's entry.
        return core::fail(core::Error::invalidInput("A plugin with this id was already added: " + id));
    }
    Entry entry;
    entry.plugin = std::move(plugin);
    entry.origin = origin;
    entries_.push_back(std::move(entry));
    return core::ok();
}

void PluginManager::reject(Entry& entry, std::string reason) {
    entry.state = PluginState::Rejected;
    entry.reason = std::move(reason);
    entry.routes.clear();
}

void PluginManager::activate(const ICapabilitySource& capabilities, ServiceRegistry& registry) {
    const CapabilitySet current = capabilities.current();
    for (Entry& entry : entries_) {
        if (entry.state == PluginState::Active || entry.state == PluginState::Rejected) {
            continue;
        }
        try {
            evaluate(entry, current, registry);
        } catch (const std::exception& e) {
            // A plugin is foreign code to the host: an exception must not take startup down.
            reject(entry, std::string("exception: ") + e.what());
        } catch (...) {
            reject(entry, "non-standard exception");
        }
        const PluginDescriptor& descriptor = entry.plugin->descriptor();
        if (entry.state == PluginState::Active) {
            core::AedstLogger::info("Plugin {} {} activated", descriptor.id, descriptor.version);
        } else {
            core::AedstLogger::warn("Plugin {} is {}: {}", descriptor.id,
                                    entry.state == PluginState::Rejected ? "rejected" : "pending", entry.reason);
        }
    }
}

void PluginManager::evaluate(Entry& entry, const CapabilitySet& capabilities, ServiceRegistry& registry) {
    const PluginDescriptor& descriptor = entry.plugin->descriptor();

    if (std::string problem = descriptorProblem(descriptor); !problem.empty()) {
        reject(entry, std::move(problem));
        return;
    }

    // Routes: validated before gating so a bad plugin is Rejected for good, not left Pending.
    std::vector<RouteContribution> routes = entry.plugin->routes();
    std::set<std::string, std::less<>> own;
    for (const RouteContribution& route : routes) {
        if (route.routeName.empty() || !isQrcUrl(route.viewUrl)) {
            reject(entry, "route '" + route.routeName + "' needs a name and a qrc:/ view");
            return;
        }
        if (!std::ranges::all_of(route.linkPrefixes, [](const std::string& p) { return isLinkPrefix(p); })) {
            reject(entry, "route '" + route.routeName + "' declares an invalid link prefix");
            return;
        }
        if (claimedRoutes_.contains(route.routeName) || !own.insert(route.routeName).second) {
            reject(entry, "route '" + route.routeName + "' is already registered");
            return;
        }
    }

    // Gating.
    if (!capabilities.isKnown() && entry.origin == PluginOrigin::Runtime) {
        entry.state = PluginState::Pending;
        entry.reason = "server capabilities are not known yet";
        return;
    }
    if (capabilities.isKnown()) {
        for (const std::string& required : descriptor.requiredCapabilities) {
            if (!capabilities.hasCapability(required)) {
                entry.state = PluginState::Pending;
                entry.reason = "missing server capability: " + required;
                return;
            }
        }
        if (!descriptor.featureFlag.empty() && !capabilities.isFlagEnabled(descriptor.featureFlag)) {
            entry.state = PluginState::Pending;
            entry.reason = "feature flag is off: " + descriptor.featureFlag;
            return;
        }
    }

    // Registration, transactional.
    ServiceRegistry snapshot = registry;
    if (auto registered = entry.plugin->registerServices(registry); !registered) {
        registry = std::move(snapshot);
        reject(entry, "registerServices failed: " + registered.error().message);
        return;
    }
    for (const RouteContribution& route : routes) {
        claimedRoutes_.insert(route.routeName);
    }
    entry.routes = std::move(routes);
    entry.state = PluginState::Active;
    entry.reason.clear();
}

bool PluginManager::isActive(std::string_view id) const {
    return std::ranges::any_of(entries_, [id](const Entry& entry) {
        return entry.state == PluginState::Active && entry.plugin->descriptor().id == id;
    });
}

std::vector<PluginStatus> PluginManager::report() const {
    std::vector<PluginStatus> out;
    out.reserve(entries_.size());
    for (const Entry& entry : entries_) {
        const PluginDescriptor& descriptor = entry.plugin->descriptor();
        out.push_back(PluginStatus{.id = descriptor.id,
                                   .version = descriptor.version,
                                   .origin = entry.origin,
                                   .state = entry.state,
                                   .reason = entry.reason});
    }
    return out;
}

std::vector<RouteContribution> PluginManager::activeRoutes() const {
    std::vector<RouteContribution> out;
    for (const Entry& entry : entries_) {
        if (entry.state == PluginState::Active) {
            out.insert(out.end(), entry.routes.begin(), entry.routes.end());
        }
    }
    return out;
}

} // namespace aedst::plugin
