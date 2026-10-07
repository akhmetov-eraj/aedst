#include "aedst/navigation/router.hpp"

#include "aedst/logging/log_categories.hpp"

#include <utility>

namespace aedst::navigation {

namespace {

/// Ids in links are server UUIDs; accept the UUID alphabet only (no '/', '?', '%').
[[nodiscard]] bool isSafeId(std::string_view id) {
    if (id.empty() || id.size() > 64) {
        return false;
    }
    for (const char c : id) {
        const bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || c == '-';
        if (!ok) {
            return false;
        }
    }
    return true;
}

/// "/<prefix>/<id>" -> id, or empty.
[[nodiscard]] std::string_view idAfter(std::string_view link, std::string_view prefix) {
    if (!link.starts_with(prefix)) {
        return {};
    }
    std::string_view id = link.substr(prefix.size());
    if (id.ends_with('/')) {
        id.remove_suffix(1);
    }
    return isSafeId(id) ? id : std::string_view{};
}

} // namespace

std::optional<Route> routeForLink(std::string_view link) {
    if (link.size() < 2 || link.front() != '/' || link[1] == '/') {
        return std::nullopt; // absolute, scheme-relative or empty
    }
    if (link == "/tasks" || link == "/tasks/") {
        return Route{.name = std::string(routes::kTasks), .params = {}};
    }
    if (const auto id = idAfter(link, "/tasks/"); !id.empty()) {
        return Route{.name = std::string(routes::kTaskDetail), .params = {{std::string(params::kTaskId), std::string(id)}}};
    }
    if (const auto id = idAfter(link, "/media/"); !id.empty()) {
        return Route{.name = std::string(routes::kMediaViewer),
                     .params = {{std::string(params::kMediaId), std::string(id)}}};
    }
    if (link == "/media" || link == "/media/") {
        return Route{.name = std::string(routes::kMedia), .params = {}};
    }
    if (link == "/notifications" || link == "/notifications/") {
        return Route{.name = std::string(routes::kNotifications), .params = {}};
    }
    return std::nullopt;
}

bool Router::openLink(const QString& link) {
    const auto route = routeForLink(link.toStdString());
    if (!route) {
        qCWarning(lcNavigation, "Router: ignoring unsupported in-app link");
        return false;
    }
    return push(*route);
}

Router::Router(std::shared_ptr<const NavGraph> graph, QObject* parent) : QObject(parent), graph_(std::move(graph)) {}

bool Router::push(const Route& route) {
    const auto view = graph_->resolve(route.name);
    if (!view) {
        qCWarning(lcNavigation, "Router: no destination registered for '%s'", route.name.c_str());
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
        qCWarning(lcNavigation, "Router: no destination registered for '%s'", route.name.c_str());
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
