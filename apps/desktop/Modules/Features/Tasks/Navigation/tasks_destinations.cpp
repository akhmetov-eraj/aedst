#include "Navigation/tasks_destinations.hpp"

#include "Data/connect_task_repository.hpp"
#include "Data/task_outbox_handlers.hpp"
#include "Navigation/tasks_route.hpp"
#include "aedst/core/logger.hpp"
#include "aedst/navigation/route.hpp"
#include "aedst/network/connect_client.hpp"
#include "aedst/outbox/outbox_handler.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace aedst::feature::tasks {
namespace {

/// Identity and screens as before, plus the feature's contribution to the offline outbox: when the host provides
/// the OutboxHandlers and the ConnectClient, queued task creates and completions are delivered by this feature's
/// handlers (the outbox core never knows a feature). Without them the feature simply works online only.
class TasksPlugin final : public plugin::IFeaturePlugin {
public:
    TasksPlugin()
        : descriptor_{.id = "tasks",
                      .version = "1.0.0",
                      .apiVersion = plugin::kPluginApiVersion,
                      .requiredCapabilities = {},
                      .featureFlag = {}} {}

    [[nodiscard]] const plugin::PluginDescriptor& descriptor() const noexcept override { return descriptor_; }

    [[nodiscard]] std::vector<plugin::RouteContribution> routes() const override {
        return {
            {std::string(navigation::routes::kTasks), std::string(kTaskListViewUrl), {"tasks"}},
            {std::string(navigation::routes::kTaskDetail), std::string(kTaskDetailViewUrl)},
        };
    }

    [[nodiscard]] core::VoidOutcome registerServices(plugin::ServiceRegistry& registry) override {
        const auto handlers = registry.find<outbox::OutboxHandlers>();
        const auto client = registry.find<network::ConnectClient>();
        if (!handlers || !client) {
            core::AedstLogger::info("Tasks: no offline outbox in this host; task changes need a connection");
            return core::ok();
        }
        const auto repository = std::make_shared<ConnectTaskRepository>(client);
        if (auto added = handlers->add(std::make_shared<TaskCreateOutboxHandler>(repository)); !added) {
            return added;
        }
        return handlers->add(std::make_shared<TaskUpdateOutboxHandler>(repository));
    }

private:
    plugin::PluginDescriptor descriptor_;
};

} // namespace

std::unique_ptr<plugin::IFeaturePlugin> makeTasksPlugin() {
    return std::make_unique<TasksPlugin>();
}

} // namespace aedst::feature::tasks
