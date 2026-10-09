#include "aedst/capabilities/capability_rules.hpp"

#include "aedst/update/semver.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace aedst::capabilities {
namespace {

using Names = plugin::CapabilitySet::Names;

std::optional<update::SemVer> parseOrNone(std::string_view text) {
    auto parsed = update::parseSemVer(text);
    if (!parsed) {
        return std::nullopt;
    }
    return std::move(*parsed);
}

/// Empty requirement = met. A requirement that cannot be parsed, or an unknown client version, is not met.
bool meetsRequirement(const std::optional<update::SemVer>& client, const std::string& required) {
    if (required.empty()) {
        return true;
    }
    const auto minimum = parseOrNone(required);
    if (!minimum || !client) {
        return false;
    }
    return update::compareSemVer(*client, *minimum) >= 0;
}

/// Server decision for one feature; `fallback` applies when the server does not list it.
bool featureOn(const ServerCapabilities* server, const std::string& id, bool fallback,
               const std::optional<update::SemVer>& client) {
    if (server == nullptr) {
        return fallback;
    }
    const auto listed = server->features().find(id);
    if (listed == server->features().end()) {
        return fallback;
    }
    return listed->second.enabled() && meetsRequirement(client, listed->second.required_client_version());
}

} // namespace

plugin::CapabilitySet evaluateCapabilities(const ClientBuild& build, const ServerCapabilities* server) {
    const auto client = parseOrNone(build.version);
    Names visible;
    Names flags;

    for (const std::string& id : build.features) {
        const bool flagged = build.flagged.contains(id);
        if (featureOn(server, id, !flagged, client)) {
            visible.insert(id);
            if (flagged) {
                flags.insert(id);
            }
        }
    }

    if (server != nullptr) {
        for (const auto& [name, feature] : server->features()) { // both are used below
            const auto dot = name.find('.');
            if (dot == std::string::npos) {
                continue; // coarse ids were handled above; coarse ids the build lacks are unknown and ignored
            }
            if (visible.contains(name.substr(0, dot)) && feature.enabled() &&
                meetsRequirement(client, feature.required_client_version())) {
                visible.insert(name);
            }
        }
    }
    return plugin::CapabilitySet::known(std::move(visible), std::move(flags));
}

VersionAdvice adviseVersion(const ClientBuild& build, const ServerCapabilities* server) {
    VersionAdvice advice;
    if (server == nullptr) {
        return advice;
    }
    if (const auto window = server->clients().find(build.platform); window != server->clients().end()) {
        advice.minimum = window->second.minimum();
        advice.recommended = window->second.recommended();
    }

    const auto client = parseOrNone(build.version);
    const auto behind = [&client](const std::string& target) {
        const auto parsed = parseOrNone(target);
        return client && parsed && update::compareSemVer(*client, *parsed) < 0;
    };

    if (behind(advice.minimum)) {
        advice.advice = UpdateAdvice::Required;
    } else if (behind(advice.recommended) ||
               server->update_status() == ::aedst::capabilities::v1::UPDATE_STATUS_RECOMMENDED) {
        advice.advice = UpdateAdvice::Recommended;
    }
    return advice;
}

} // namespace aedst::capabilities
