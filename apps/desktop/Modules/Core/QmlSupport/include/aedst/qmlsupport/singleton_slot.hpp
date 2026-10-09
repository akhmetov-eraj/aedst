#pragma once

#include <QJSEngine>
#include <QQmlEngine>

namespace aedst::qmlsupport {

/// Holds the one instance a `QML_SINGLETON` type hands to the QML engine. A singleton class declares
///
///     QML_ELEMENT
///     QML_SINGLETON
///     ...
///     static Foo* create(QQmlEngine* engine, QJSEngine* script) {
///         return aedst::qmlsupport::SingletonSlot<Foo>::create(engine, script);
///     }
///
/// and the composition root calls `SingletonSlot<Foo>::provide(&foo)` before the QML is loaded (see
/// App/src/qml_singletons.cpp). The instance stays owned by C++ (AppDependencies); the engine never deletes it.
template <typename T>
class SingletonSlot final {
public:
    static void provide(T* instance) noexcept { instance_ = instance; }

    [[nodiscard]] static T* create(QQmlEngine* /*engine*/, QJSEngine* /*script*/) {
        if (instance_ != nullptr) {
            QJSEngine::setObjectOwnership(instance_, QJSEngine::CppOwnership);
        }
        return instance_;
    }

private:
    static inline T* instance_{nullptr};
};

} // namespace aedst::qmlsupport
