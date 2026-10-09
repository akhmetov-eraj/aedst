#pragma once

#include "aedst/plugin/feature_plugin.hpp"

#include <QtPlugin>

#include <memory>

namespace aedst::plugin {

/// What a run-time plugin library exports to Qt's plugin system. The library's root object is a
/// QObject that also implements this interface:
///
///     class ReportsPlugin final : public QObject, public aedst::plugin::QtFeaturePluginFactory {
///         Q_OBJECT
///         Q_PLUGIN_METADATA(IID "com.aedst.FeaturePlugin/1" FILE "reports_plugin.json")
///         Q_INTERFACES(aedst::plugin::QtFeaturePluginFactory)
///     public:
///         std::unique_ptr<aedst::plugin::IFeaturePlugin> create() override;
///     };
///
/// with `reports_plugin.json` = {"id": "vendor.reports", "apiVersion": 1}. The IID carries the API
/// version: when kPluginApiVersion changes, the suffix changes with it, so Qt itself refuses a
/// stale library before any of its code runs.
class QtFeaturePluginFactory {
public:
    QtFeaturePluginFactory() = default;
    QtFeaturePluginFactory(const QtFeaturePluginFactory&) = delete;
    QtFeaturePluginFactory& operator=(const QtFeaturePluginFactory&) = delete;
    QtFeaturePluginFactory(QtFeaturePluginFactory&&) = delete;
    QtFeaturePluginFactory& operator=(QtFeaturePluginFactory&&) = delete;
    virtual ~QtFeaturePluginFactory() = default;

    [[nodiscard]] virtual std::unique_ptr<IFeaturePlugin> create() = 0;
};

} // namespace aedst::plugin

QT_BEGIN_NAMESPACE
Q_DECLARE_INTERFACE(aedst::plugin::QtFeaturePluginFactory, "com.aedst.FeaturePlugin/1")
QT_END_NAMESPACE
