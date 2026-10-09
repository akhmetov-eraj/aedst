#pragma once

#include "aedst/capabilities/capability_rules.hpp"
#include "aedst/core/outcome.hpp"
#include "aedst/network/connect_client.hpp"
#include "aedst/persistence/database_worker.hpp"

#include <memory>
#include <optional>
#include <string>

namespace aedst::capabilities {

/// Where the server's answer comes from. Production: ConnectCapabilitiesRemote; tests: a scripted fake.
/// The completion runs exactly once, asynchronously, on the GUI thread.
class CapabilitiesRemote {
public:
    CapabilitiesRemote() = default;
    CapabilitiesRemote(const CapabilitiesRemote&) = delete;
    CapabilitiesRemote& operator=(const CapabilitiesRemote&) = delete;
    CapabilitiesRemote(CapabilitiesRemote&&) = delete;
    CapabilitiesRemote& operator=(CapabilitiesRemote&&) = delete;
    virtual ~CapabilitiesRemote() = default;

    virtual void fetch(core::Completion<ServerCapabilities> done) = 0;
};

/// Where the last-known answer survives a restart. The stored value is the serialized
/// GetCapabilitiesResponse (opaque to the cache); a missing value is `nullopt`, not an error.
class CapabilityCache {
public:
    CapabilityCache() = default;
    CapabilityCache(const CapabilityCache&) = delete;
    CapabilityCache& operator=(const CapabilityCache&) = delete;
    CapabilityCache(CapabilityCache&&) = delete;
    CapabilityCache& operator=(CapabilityCache&&) = delete;
    virtual ~CapabilityCache() = default;

    virtual void load(core::Completion<std::optional<std::string>> done) = 0;
    virtual void save(std::string serialized, core::Completion<void> done) = 0;
};

/// GetCapabilities over the app's ConnectClient. The RPC is public (no token) and idempotent, so the client's
/// RetryPolicy may retry it; the X-Aedst-Client header identifies this build.
class ConnectCapabilitiesRemote final : public CapabilitiesRemote {
public:
    explicit ConnectCapabilitiesRemote(std::shared_ptr<network::ConnectClient> client);

    void fetch(core::Completion<ServerCapabilities> done) override;

private:
    std::shared_ptr<network::ConnectClient> client_;
};

/// CapabilityCache in the `kv` table on the app's DatabaseWorker (the database thread; never the GUI thread).
class SqliteCapabilityCache final : public CapabilityCache {
public:
    SqliteCapabilityCache(std::shared_ptr<persistence::DatabaseWorker> worker, persistence::Dispatcher dispatcher);

    void load(core::Completion<std::optional<std::string>> done) override;
    void save(std::string serialized, core::Completion<void> done) override;

private:
    std::shared_ptr<persistence::DatabaseWorker> worker_;
    persistence::Dispatcher dispatcher_;
};

} // namespace aedst::capabilities
