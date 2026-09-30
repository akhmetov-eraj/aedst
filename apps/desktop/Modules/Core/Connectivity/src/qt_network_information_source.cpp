#include "aedst/connectivity/reachability_source.hpp"

#include <QNetworkInformation>

namespace aedst::connectivity {

QtNetworkInformationSource::QtNetworkInformationSource(QObject* parent) : ReachabilitySource(parent) {
    // Loads the platform backend plugin (NetworkManager, Network List Manager,
    // SCNetworkReachability, ...). Fails softly when none is deployed.
    if (QNetworkInformation::instance() == nullptr && !QNetworkInformation::loadDefaultBackend()) {
        return;
    }
    auto* info = QNetworkInformation::instance();
    if (info == nullptr || !info->supports(QNetworkInformation::Feature::Reachability)) {
        return;
    }
    connect(info, &QNetworkInformation::reachabilityChanged, this, [this]() { emit reachabilityChanged(); });
}

bool QtNetworkInformationSource::isAvailable() const noexcept {
    const auto* info = QNetworkInformation::instance();
    return info != nullptr && info->supports(QNetworkInformation::Feature::Reachability);
}

Reachability QtNetworkInformationSource::reachability() const {
    if (!isAvailable()) {
        return Reachability::Unknown;
    }
    switch (QNetworkInformation::instance()->reachability()) {
    case QNetworkInformation::Reachability::Disconnected:
        return Reachability::Offline;
    case QNetworkInformation::Reachability::Site:
    case QNetworkInformation::Reachability::Online:
        return Reachability::Online;
    case QNetworkInformation::Reachability::Unknown:
    case QNetworkInformation::Reachability::Local:
        // Local-only may still reach a LAN/localhost API server: do not block the user.
        break;
    }
    return Reachability::Unknown;
}

} // namespace aedst::connectivity
