#include "aedst/navigation/router.hpp"

#include "aedst/core/logger.hpp"

#include <utility>

namespace aedst::navigation {

using core::AedstLogger;

Router::Router(std::shared_ptr<const NavGraph> graph, QObject* parent) : QObject(parent), graph_(std::move(graph)) {}

bool Router::push(const Route& route) {
    const auto view = graph_->resolve(route.name);
    if (!view) {
        AedstLogger::warn("Router: no destination registered for '{}'", route.name);
        return false;
    }
    backStack_.push_back(route);
    emit pushRequested(*view, toProperties(route));
    emit currentRouteChanged();
    return true;
}

bool Router::resetTo(const Route& route) {
    const auto view = graph_->resolve(route.name);
    if (!view) {
        AedstLogger::warn("Router: no destination registered for '{}'", route.name);
        return false;
    }
    backStack_.clear();
    backStack_.push_back(route);
    emit replaceAllRequested(*view, toProperties(route));
    emit currentRouteChanged();
    return true;
}

bool Router::navigate(const QString& routeName, const QVariantMap& params) {
    return push(toRoute(routeName, params));
}

bool Router::replaceAll(const QString& routeName, const QVariantMap& params) {
    return resetTo(toRoute(routeName, params));
}

bool Router::goBack() {
    if (backStack_.size() <= 1) {
        return false;
    }
    backStack_.pop_back();
    emit popRequested();
    emit currentRouteChanged();
    return true;
}

QString Router::currentRoute() const {
    return backStack_.empty() ? QString{} : QString::fromStdString(backStack_.back().name);
}

int Router::depth() const {
    return static_cast<int>(backStack_.size());
}

const std::vector<Route>& Router::backStack() const noexcept {
    return backStack_;
}

Route Router::toRoute(const QString& routeName, const QVariantMap& params) {
    Route route{.name = routeName.toStdString(), .params = {}};
    for (auto it = params.cbegin(); it != params.cend(); ++it) {
        route.params.emplace(it.key().toStdString(), it.value().toString().toStdString());
    }
    return route;
}

QVariantMap Router::toProperties(const Route& route) {
    QVariantMap properties;
    for (const auto& [key, value] : route.params) {
        properties.insert(QString::fromStdString(key), QString::fromStdString(value));
    }
    return properties;
}

} // namespace aedst::navigation
