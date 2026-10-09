#pragma once

#include "aedst/core/outcome.hpp"
#include "aedst/plugin/feature_plugin.hpp"
#include "aedst/plugin/plugin_trust.hpp"
#include "aedst/update/manifest_verifier.hpp"

#include <QByteArray>
#include <QtGlobal>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class QPluginLoader;

namespace aedst::plugin {

inline constexpr const char* kAllowlistFileName = "plugins.allowlist.json";
inline constexpr const char* kAllowlistSignatureFileName = "plugins.allowlist.sig";
/// Upper bounds, so a hostile directory cannot make the loader read unbounded data.
inline constexpr qint64 kMaxAllowlistBytes = 256 * 1024;
inline constexpr qint64 kMaxPluginFileBytes = 64LL * 1024 * 1024;
inline constexpr int kMaxPluginFiles = 64;

struct PluginLoaderConfig {
    /// The ONLY directory plugins are loaded from. Never taken from the environment or from the
    /// command line; the composition root passes `<application dir>/plugins`.
    std::filesystem::path directory;
    /// Keys that may sign the allowlist (the release keys compiled into the binary).
    std::vector<update::TrustedKey> trustedKeys;
    /// True in every NDEBUG build.
    bool releaseBuild{true};
    /// Debug builds only; ignored in release.
    bool allowUnsignedDevPlugins{false};

    /// Directory as given, trusted keys = update::embeddedTrustedKeys(), releaseBuild from NDEBUG,
    /// and the unsigned-dev switch from AEDST_ALLOW_UNSIGNED_PLUGINS=1, which is read in Debug
    /// builds only (a release build never looks at it).
    [[nodiscard]] static PluginLoaderConfig forApplication(std::filesystem::path directory);
};

/// One line of the load log: what happened to one file (or to the directory as a whole, in which
/// case `fileName` is empty).
struct LoadReport {
    std::string fileName;
    TrustVerdict verdict{TrustVerdict::NoAllowlist};
    bool loaded{false};
    std::string detail;
};

/// A created plugin. Destroy it before the PluginLoader that produced it: the library stays loaded
/// until process exit (QPluginLoader::unload() is never called), but the loader owns the bookkeeping.
struct LoadedPlugin {
    std::unique_ptr<IFeaturePlugin> plugin;
    std::string fileName;
};

/// Parses plugins.allowlist.json:
///   {"schema": 1, "plugins": [{"file": "x.dll", "sha256": "<64 hex>", "size": 123, "id": "vendor.x"}]}
/// Strict: unknown schema, a malformed entry, a duplicate file name or an invalid plugin id fail the
/// whole file (an allowlist is all-or-nothing).
[[nodiscard]] core::Outcome<std::vector<AllowlistEntry>> parseAllowlist(const QByteArray& json);

/// Discovers and loads run-time plugins under the trust rules of ADR-0033. It never throws and
/// never loads a file whose evaluateCandidate() decision is not loadable().
class PluginLoader final {
public:
    explicit PluginLoader(PluginLoaderConfig config);
    PluginLoader(const PluginLoader&) = delete;
    PluginLoader& operator=(const PluginLoader&) = delete;
    PluginLoader(PluginLoader&&) = delete;
    PluginLoader& operator=(PluginLoader&&) = delete;
    ~PluginLoader();

    /// Returns the created plugins; the manager still validates and gates them. `report` gets one
    /// entry per candidate file (and one for a directory-level refusal).
    [[nodiscard]] std::vector<LoadedPlugin> loadAll(std::vector<LoadReport>& report);

private:
    PluginLoaderConfig config_;
    std::vector<std::unique_ptr<QPluginLoader>> loaders_;
};

} // namespace aedst::plugin
