#pragma once

namespace aedst::connectivity {
class ConnectivityMonitor;
} // namespace aedst::connectivity

namespace aedst::platform {
class InAppNotificationService;
} // namespace aedst::platform

namespace aedst::app {

class AppDependencies;

/// Hands every object that QML reaches as a singleton to its `QML_SINGLETON` slot
/// (aedst::qmlsupport::SingletonSlot<T>). The types register themselves declaratively through
/// qt_add_qml_module (QML_ELEMENT/QML_SINGLETON); this is the only place that supplies the instances, and it must run
/// before the QML engine loads a file. The instances stay owned by AppDependencies / main().
void provideQmlSingletons(AppDependencies& deps, connectivity::ConnectivityMonitor& connectivity,
                          platform::InAppNotificationService& notifications);

} // namespace aedst::app
