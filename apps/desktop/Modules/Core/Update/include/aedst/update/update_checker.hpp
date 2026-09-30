#pragma once

#include "aedst/core/outcome.hpp"
#include "aedst/update/http_fetcher.hpp"
#include "aedst/update/manifest.hpp"
#include "aedst/update/manifest_verifier.hpp"
#include "aedst/update/update_state_store.hpp"

#include <cstddef>
#include <optional>
#include <string>

namespace aedst::update {

struct UpdateCheckerConfig {
    /// https URL of the signed manifest envelope.
    std::string manifestUrl;
    /// Version of the running build (semver).
    std::string installedVersion;
    /// Only manifests of this channel are accepted (blocks channel confusion).
    std::string channel{"stable"};
    /// Must match ArtifactInfo::platform / arch of the manifest ("windows", "x86_64", ...).
    std::string platform;
    std::string arch;
    /// Offer a manifest whose version is older than the installed one (explicit
    /// "switch channel / roll back" flow). Off by default: an older manifest is
    /// then rejected as a rollback attempt.
    bool allowDowngrade{false};
    std::size_t maxManifestBytes{256U * 1024U};
};

struct UpdateOffer {
    UpdateManifest manifest;
    /// The artifact for this platform/arch; its url/sha256/size were verified as
    /// part of the signed manifest but the file itself is NOT downloaded here.
    ArtifactInfo artifact;
    /// installedVersion < manifest.minSupportedVersion.
    bool mandatory{false};
    bool isDowngrade{false};
};

/// Fetches, verifies and evaluates the update manifest. It never downloads or
/// installs anything: the caller downloads `artifact.url` and must pass the file
/// through ArtifactVerifier::verifyFile() before handing it to an installer.
///
/// Checks, in order: config sanity (https manifest URL, semver), fetch (size
/// capped), Ed25519 signature (ManifestVerifier), channel match, replay
/// protection (publishedAt and version must not go backwards vs the persisted
/// UpdateState), then persists the new state, then version comparison vs the
/// installed build (== up to date, < rollback error unless allowDowngrade).
class UpdateChecker final {
public:
    /// All references must outlive the checker.
    UpdateChecker(UpdateCheckerConfig config, HttpFetcher& fetcher, const ManifestVerifier& verifier,
                  UpdateStateStore& stateStore);

    /// nullopt = already up to date.
    [[nodiscard]] core::Outcome<std::optional<UpdateOffer>> check();

private:
    UpdateCheckerConfig config_;
    HttpFetcher& fetcher_;
    const ManifestVerifier& verifier_;
    UpdateStateStore& stateStore_;
};

} // namespace aedst::update
