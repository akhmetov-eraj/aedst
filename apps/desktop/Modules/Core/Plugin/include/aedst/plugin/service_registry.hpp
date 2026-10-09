#pragma once

#include "aedst/core/outcome.hpp"

#include <memory>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>

namespace aedst::plugin {

/// Type-keyed bag of services that the host and plugins exchange during registration.
///
/// This is deliberately NOT the app's DI container (ARCHITECTURE.md rule 4: everything is wired
/// manually in AppDependencies). It is the narrow contract between the composition root and a
/// plugin: the host provides the few Core services a plugin may use (for example the
/// ConnectClient), a plugin provides what it contributes (for example a view-model factory). A
/// service can be provided once; a second provide() of the same type is an error, so a plugin can
/// never replace a host service.
///
/// Copyable on purpose: PluginManager snapshots it before a plugin registers and restores the
/// snapshot when the registration fails, so a failed plugin leaves nothing behind.
/// Not thread-safe; used on the GUI thread during startup.
class ServiceRegistry final {
public:
    template <typename T>
    [[nodiscard]] core::VoidOutcome provide(std::shared_ptr<T> service) {
        if (!service) {
            return core::fail(core::Error::invalidInput("Cannot provide a null service"));
        }
        const auto [it, inserted] = services_.try_emplace(std::type_index(typeid(T)), std::move(service));
        if (!inserted) {
            return core::fail(core::Error::invalidInput(std::string("Service already provided: ") + typeid(T).name()));
        }
        return core::ok();
    }

    /// nullptr when no service of type T was provided.
    template <typename T>
    [[nodiscard]] std::shared_ptr<T> find() const {
        const auto it = services_.find(std::type_index(typeid(T)));
        if (it == services_.end()) {
            return nullptr;
        }
        return std::static_pointer_cast<T>(it->second);
    }

    template <typename T>
    [[nodiscard]] bool contains() const {
        return services_.contains(std::type_index(typeid(T)));
    }

    [[nodiscard]] std::size_t size() const noexcept { return services_.size(); }

private:
    std::unordered_map<std::type_index, std::shared_ptr<void>> services_;
};

} // namespace aedst::plugin
