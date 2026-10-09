include(AedstCompilerWarnings)

find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Core Qml)

# Header-only helper for declarative QML singletons (QML_ELEMENT + QML_SINGLETON + static create()): the
# composition root hands each already-built instance over with SingletonSlot<T>::provide(). Replaces the run-time
# qmlRegisterSingleton*() calls (docs/performance.md, "Declarative type registration"; enforced by
# scripts/check-forbidden-apis.sh, rule qml-register).
add_library(AedstQmlSupport INTERFACE)
add_library(Aedst::QmlSupport ALIAS AedstQmlSupport)

target_include_directories(AedstQmlSupport INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(AedstQmlSupport INTERFACE Qt6::Core Qt6::Qml)
