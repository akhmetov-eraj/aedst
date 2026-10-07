#pragma once

#include "aedst/core/outcome.hpp"
#include "aedst/update/http_fetcher.hpp"
#include "aedst/update/manifest_verifier.hpp"
#include "aedst/update/update_checker.hpp"
#include "aedst/update/update_state_store.hpp"
#include "aedst/updater/update_installer.hpp"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>

namespace aedst::updater {

enum class PipelineStage {
    CheckingManifest,
    CheckingFeed,
    Downloading,
    Verifying,
};

struct PipelineEvents {
    /// Called on the worker thread.
    std::function<void(PipelineStage)> stage;
    std::function<void(int percent)> progress;
};

/// The result of a successful run with something to install or to announce.
struct UpdateOutcome {
    update::UpdateOffer offer;
    /// Empty when this install cannot update itself (package-manager / Flatpak / portable): the offer is only reported.
    std::optional<InstallerCandidate> candidate;
    /// The verified package on disk (when the installer exposes it).
    std::filesystem::path package;

    [[nodiscard]] bool readyToInstall() const noexcept { return candidate.has_value(); }
};

/// check -> bind -> download -> verify, as one blocking function (run it on a worker thread):
///
///  1. UpdateChecker: signed manifest, channel, replay and rollback protection (Ed25519, ADR-0006).
///  2. The installer's own feed must offer exactly what the signed manifest names: the same version, the same SHA-256
///     and size. A feed that disagrees - a compromised or misconfigured update server - is refused and never downloaded
///     from; this is what ties the installer's transport to the release signature.
///  3. Download through the installer, then hash the file again with ArtifactVerifier when the installer exposes its
///     path (size and SHA-256 against the signed manifest, constant-time comparison).
///
/// nullopt = already up to date. Installs nothing; the caller decides when to call UpdateInstaller::applyAfterExit.
[[nodiscard]] core::Outcome<std::optional<UpdateOutcome>>
runUpdatePipeline(const update::UpdateCheckerConfig& config, update::HttpFetcher& fetcher,
                  const update::ManifestVerifier& verifier, update::UpdateStateStore& stateStore,
                  UpdateInstaller& installer, const PipelineEvents& events = {});

/// True when two versions are the same release (semver precedence; build metadata ignored).
[[nodiscard]] bool sameVersion(const std::string& a, const std::string& b);

} // namespace aedst::updater
