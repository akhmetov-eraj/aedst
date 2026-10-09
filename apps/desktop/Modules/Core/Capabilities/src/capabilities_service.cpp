#include "aedst/capabilities/capabilities_service.hpp"

#include "aedst/logging/log_categories.hpp"

#include <QPointer>

#include <algorithm>
#include <utility>

namespace aedst::capabilities {

CapabilitiesService::CapabilitiesService(Dependencies dependencies, QObject* parent)
    : QObject(parent),
      remote_(std::move(dependencies.remote)),
      cache_(std::move(dependencies.cache)),
      build_(std::move(dependencies.build)),
      visible_(evaluateCapabilities(build_, nullptr)) {}

CapabilitiesService::~CapabilitiesService() = default;

void CapabilitiesService::start() {
    loadCache();
    refresh();
}

void CapabilitiesService::loadCache() {
    if (!cache_) {
        return;
    }
    cache_->load([self = QPointer<CapabilitiesService>(this)](core::Outcome<std::optional<std::string>> loaded) {
        // A fetch that already answered is newer than whatever the cache holds.
        if (self.isNull() || self->fetchedOnce_ || !loaded || !loaded->has_value()) {
            return;
        }
        ServerCapabilities cached;
        if (!cached.ParseFromString(**loaded)) {
            qCWarning(lcApp, "Ignoring an unreadable cached capabilities answer");
            return;
        }
        self->applyAnswer(std::move(cached), false);
    });
}

void CapabilitiesService::refresh() {
    if (fetching_ || !remote_) {
        return;
    }
    fetching_ = true;
    remote_->fetch([self = QPointer<CapabilitiesService>(this)](core::Outcome<ServerCapabilities> fetched) {
        if (self.isNull()) {
            return;
        }
        self->fetching_ = false;
        if (!fetched) {
            // Keep whatever is known (last answer, else the defaults). An upgrade-required refusal is reported by
            // ConnectClient::clientUpgradeRequired, which the app already routes to the upgrade screen.
            qCWarning(lcNetwork, "Fetching server capabilities failed: %s", fetched.error().message.c_str());
            return;
        }
        self->fetchedOnce_ = true;
        self->applyAnswer(std::move(*fetched), true);
    });
}

void CapabilitiesService::applyAnswer(ServerCapabilities answer, bool persist) {
    visible_ = evaluateCapabilities(build_, &answer);
    advice_ = adviseVersion(build_, &answer);
    if (persist && cache_) {
        cache_->save(answer.SerializeAsString(), [](core::Outcome<void> saved) {
            if (!saved) {
                qCWarning(lcApp, "Could not cache the server capabilities: %s", saved.error().message.c_str());
            }
        });
    }
    answer_ = std::move(answer);
    current_ = visible_;
    emit capabilitiesChanged();

    if (advice_.advice == UpdateAdvice::Required && !upgradeSignalled_) {
        upgradeSignalled_ = true;
        emit upgradeRequired();
    } else if (advice_.advice == UpdateAdvice::Recommended && !recommendedSignalled_) {
        recommendedSignalled_ = true;
        emit upgradeRecommended();
    }
}

bool CapabilitiesService::isEnabled(std::string_view id) const {
    return visible_.hasCapability(id);
}

std::vector<std::string> CapabilitiesService::visibleFeatures() const {
    std::vector<std::string> names;
    for (const std::string& id : build_.features) {
        if (visible_.hasCapability(id)) {
            names.push_back(id);
        }
    }
    if (answer_) {
        for (const auto& entry : answer_->features()) {
            const std::string& name = entry.first;
            if (name.find('.') != std::string::npos && visible_.hasCapability(name)) {
                names.push_back(name);
            }
        }
    }
    std::ranges::sort(names);
    return names;
}

} // namespace aedst::capabilities
