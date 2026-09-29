#pragma once

#include "aedst/navigation/nav_graph.hpp"
#include "aedst/navigation/route.hpp"

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantMap>

#include <memory>
#include <vector>

namespace aedst::navigation {

/// Owns the back stack; QML's StackView in App/qml/Main.qml mirrors it by
/// reacting to the *Requested signals. ViewModels hold a Router* and call
/// push()/resetTo() with a Route — they never see QML or another feature.
class Router final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentRoute READ currentRoute NOTIFY currentRouteChanged)
    Q_PROPERTY(int depth READ depth NOTIFY currentRouteChanged)

public:
    explicit Router(std::shared_ptr<const NavGraph> graph, QObject* parent = nullptr);

    /// Returns false (and logs) when no destination is registered for the route.
    bool push(const Route& route);
    bool resetTo(const Route& route);

    Q_INVOKABLE bool navigate(const QString& routeName, const QVariantMap& params = {});
    Q_INVOKABLE bool replaceAll(const QString& routeName, const QVariantMap& params = {});
    Q_INVOKABLE bool goBack();

    [[nodiscard]] QString currentRoute() const;
    [[nodiscard]] int depth() const;
    [[nodiscard]] const std::vector<Route>& backStack() const noexcept;

signals:
    void pushRequested(const QUrl& view, const QVariantMap& properties);
    void popRequested();
    void replaceAllRequested(const QUrl& view, const QVariantMap& properties);
    void currentRouteChanged();

private:
    [[nodiscard]] static Route toRoute(const QString& routeName, const QVariantMap& params);
    [[nodiscard]] static QVariantMap toProperties(const Route& route);

    std::shared_ptr<const NavGraph> graph_;
    std::vector<Route> backStack_;
};

} // namespace aedst::navigation
