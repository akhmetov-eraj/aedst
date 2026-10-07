#include "aedst/update/update_checker.hpp"

#include "aedst/update/semver.hpp"
#include "aedst/update/url_validation.hpp"

#include <compare>
#include <optional>
#include <string>
#include <utility>

namespace aedst::update {

using core::Error;
using core::fail;

UpdateChecker::UpdateChecker(UpdateCheckerConfig config, HttpFetcher& fetcher, const ManifestVerifier& verifier,
                             UpdateStateStore& stateStore)
    : config_(std::move(config)), fetcher_(fetcher), verifier_(verifier), stateStore_(stateStore) {}

core::Outcome<std::optional<UpdateOffer>> UpdateChecker::check() {
    if (!isSecureHttpsUrl(config_.manifestUrl)) {
        return fail(Error::invalidArgument("update: manifest URL must be a valid https URL"));
    }
    const auto installed = parseSemVer(config_.installedVersion);
    if (!installed) {
        return fail(Error::invalidArgument("update: installed version is not valid semver"));
    }
    if (config_.platform.empty() || config_.arch.empty()) {
        return fail(Error::invalidArgument("update: platform and arch are required"));
    }

    auto body = fetcher_.fetch(config_.manifestUrl, config_.maxManifestBytes);
    if (!body) {
        return fail(body.error());
    }
    if (body->size() > config_.maxManifestBytes) {
        return fail(Error::invalidArgument("update: manifest response too large"));
    }

    auto manifest = verifier_.verify(*body);
    if (!manifest) {
        return fail(manifest.error());
    }
    if (manifest->channel != config_.channel) {
        return fail(Error::invalidArgument("update: manifest is for a different channel"));
    }
    // parseManifest() already validated these, so failure here is a logic error.
    const auto offered = parseSemVer(manifest->version);
    const auto minimum = parseSemVer(manifest->minSupportedVersion);
    if (!offered || !minimum) {
        return fail(Error::invalidArgument("update: manifest versions are invalid"));
    }

    // Replay / rollback protection against what this install has seen before.
    auto state = stateStore_.load();
    if (!state) {
        return fail(state.error());
    }
    if (manifest->publishedAt < state->lastSeenPublishedAt) {
        return fail(Error::invalidArgument("update: manifest is older than one already seen (replay)"));
    }
    if (!state->lastSeenVersion.empty()) {
        const auto lastSeen = parseSemVer(state->lastSeenVersion);
        // A signed rollback may move the version backwards; its publishedAt is still checked above.
        if (!manifest->rollback && lastSeen && compareSemVer(*offered, *lastSeen) < 0) {
            return fail(Error::invalidArgument("update: manifest version is older than one already seen (rollback)"));
        }
    }
    const UpdateState next{manifest->publishedAt, manifest->version};
    if (auto saved = stateStore_.save(next); !saved) {
        return fail(saved.error());
    }

    const std::strong_ordering order = compareSemVer(*offered, *installed);
    if (order == 0) {
        return std::optional<UpdateOffer>{};
    }
    const bool isDowngrade = order < 0;
    if (isDowngrade && !config_.allowDowngrade && !manifest->rollback) {
        return fail(Error::invalidArgument("update: manifest is older than the installed version (rollback rejected)"));
    }

    const ArtifactInfo* artifact = findArtifact(*manifest, config_.platform, config_.arch);
    if (artifact == nullptr) {
        return fail(Error::notFound("update: no artifact for this platform and architecture"));
    }

    UpdateOffer offer;
    offer.artifact = *artifact;
    offer.rollback = manifest->rollback;
    offer.isDowngrade = isDowngrade;
    // Moving away from a release the publisher withdrew is not optional.
    offer.mandatory = compareSemVer(*installed, *minimum) < 0 || (manifest->rollback && isDowngrade);
    offer.manifest = std::move(*manifest);
    return std::optional<UpdateOffer>{std::move(offer)};
}

} // namespace aedst::update
