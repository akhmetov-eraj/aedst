#pragma once

#include <QPointer>
#include <QQuickWindow>
#include <QStringList>

namespace aedst::app {

class AppDependencies;

/// Acts on any deep link among `arguments` and, when a window is given, brings it to the front.
///
/// Every argument is validated against the allow-list of the ACTIVE routes (platform::parseDeepLink with
/// Router::linkPrefixes()) before anything acts on it, and handed to AppDependencies::openDeepLink, which opens it at
/// once when signed in and otherwise keeps it until sign-in completes. Arguments were already stripped of control
/// characters by SingleInstanceGuard. `deps` may be null (only raise the window).
void handleActivation(const QStringList& arguments, const QPointer<QQuickWindow>& window, AppDependencies* deps);

} // namespace aedst::app
