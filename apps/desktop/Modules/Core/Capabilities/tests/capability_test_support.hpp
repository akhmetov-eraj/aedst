#pragma once

#include "aedst/capabilities/capability_ports.hpp"
#include "aedst/capabilities/capability_rules.hpp"

#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <utility>

namespace aedst::capabilities::testsupport {

/// The build used by the shared fixture's rules (docs/contract/behavior/capabilities.json "clientBuild").
inline ClientBuild fixtureBuild(std::string version = "1.3.0") {
    return ClientBuild{.platform = "android",
                       .version = std::move(version),
                       .features = {"tasks", "media", "search", "admin", "notifications", "auth"},
                       .flagged = {"admin"}};
}

inline void addFeature(ServerCapabilities& response, const std::string& name, bool enabled,
                       const std::string& requiredClientVersion = {}) {
    auto& feature = (*response.mutable_features())[name];
    feature.set_enabled(enabled);
    feature.set_required_client_version(requiredClientVersion);
}

inline void setWindow(ServerCapabilities& response, const std::string& platform, const std::string& minimum,
                      const std::string& recommended) {
    auto& window = (*response.mutable_clients())[platform];
    window.set_minimum(minimum);
    window.set_recommended(recommended);
}

/// Answers fetch() by hand: the test decides when and how each request completes.
class ScriptedRemote final : public CapabilitiesRemote {
public:
    void fetch(core::Completion<ServerCapabilities> done) override {
        ++fetchCount;
        pending.push_back(std::move(done));
    }

    void answer(ServerCapabilities response) {
        auto done = std::move(pending.front());
        pending.pop_front();
        done(std::move(response));
    }

    void fail(core::Error error = core::Error::unavailable("offline")) {
        auto done = std::move(pending.front());
        pending.pop_front();
        done(core::fail(std::move(error)));
    }

    int fetchCount{0};
    std::deque<core::Completion<ServerCapabilities>> pending;
};

/// In-memory cache that completes synchronously.
class MemoryCache final : public CapabilityCache {
public:
    void load(core::Completion<std::optional<std::string>> done) override {
        ++loadCount;
        done(stored);
    }

    void save(std::string serialized, core::Completion<void> done) override {
        stored = std::move(serialized);
        ++saveCount;
        done(core::ok());
    }

    std::optional<std::string> stored;
    int loadCount{0};
    int saveCount{0};
};

} // namespace aedst::capabilities::testsupport
