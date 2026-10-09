#pragma once

#include "aedst/capabilities/capability_rules.hpp"

#include <string>
#include <utility>

namespace aedst::app {

/// What this desktop build contains, for capability negotiation (ADR-0035). The ids are the server's coarse module ids
/// (apps/server/api/proto/aedst/capabilities/v1/capabilities.proto). One entry per feature plugin registered by
/// PluginHost (its descriptor id is the same id); add an id here when a feature is added. Only `admin` is flagged
/// (default-off until the server enables it), the same set as on every other client
/// (docs/content/decisions/cross-cutting.md); the rest are built-ins shown unless the server switches them off.
[[nodiscard]] inline capabilities::ClientBuild desktopClientBuild(std::string version) {
    return capabilities::ClientBuild{
        .platform = "desktop",
        .version = std::move(version),
        .features = {"auth", "tasks", "search", "notifications", "media", "admin"},
        .flagged = {"admin"},
    };
}

} // namespace aedst::app
