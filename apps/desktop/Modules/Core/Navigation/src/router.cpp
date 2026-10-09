#include "aedst/navigation/router.hpp"

#include "aedst/logging/log_categories.hpp"

#include <algorithm>
#include <span>
#include <string>
#include <utility>
#include <vector>

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

std::optional<Route> routeForLink(std::string_view link, std::span<const std::string> linkPrefixes) {
    if (link.size() < 2 || link.front() != '/' || link[1] == '/') {
        return std::nullopt; // absolute, scheme-relative or empty
    }
    // First path segment ("/tasks/<id>" -> "tasks"): it must be claimed by an active route.
    const std::string_view rest = link.substr(1);
    const std::string_view prefix = rest.substr(0, rest.find('/'));
    if (std::ranges::find(linkPrefixes, prefix) == linkPrefixes.end()) {
        return std::nullopt;
    }
    if (prefix == "tasks") {
        if (link == "/tasks" || link == "/tasks/") {
            return routes::tasks();
        }
        if (const auto id = idAfter(link, "/tasks/"); !id.empty()) {
            return routes::taskDetail(id);
        }
    } else if (prefix == "media") {
        if (const auto id = idAfter(link, "/media/"); !id.empty()) {
            return routes::mediaViewer(id);
        }
        if (link == "/media" || link == "/media/") {
            return routes::media();
        }
    } else if (prefix == "notifications") {
        if (link == "/notifications" || link == "/notifications/") {
            return routes::notifications();
        }
    }
    return std::nullopt;
}

std::vector<std::string> Router::linkPrefixes() const {
    return graph_->linkPrefixes();
}

bool Router::openLink(const QString& link) {
    const auto prefixes = graph_->linkPrefixes();
    const auto route = routeForLink(link.toStdString(), prefixes);
    if (!route) {
        qCWarning(lcNavigation, "Router: ignoring unsupported in-app link");
        return false;
    }
    return push(*route);
}

Router::Router(std::shared_ptr<const NavGraph> graph, QObject* parent) : QObject(parent), graph_(std::move(graph)) {}

bool Router::push(const Route& route) {
    if (locked_) {
        qCInfo(lcNavigation, "Router: locked, ignoring push to '%s'", route.name.c_str());
        return false;
    }
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
    if (locked_) {
        qCInfo(lcNavigation, "Router: locked, ignoring reset to '%s'", route.name.c_str());
        return false;
    }
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

bool Router::lockTo(const Route& route) {
    if (!resetTo(route)) {
        return false;
    }
    locked_ = true;
    return true;
}

bool Router::navigate(const QString& routeName, const QVariantMap& params) {
    return push(toRoute(routeName, params));
}

bool Router::replaceAll(const QString& routeName, const QVariantMap& params) {
    return resetTo(toRoute(routeName, params));
}

bool Router::goBack() {
    if (locked_) {
        return false;
    }
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
