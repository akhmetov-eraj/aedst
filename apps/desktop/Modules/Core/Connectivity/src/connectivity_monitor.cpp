#include "aedst/connectivity/connectivity_monitor.hpp"

#include <utility>

namespace aedst::connectivity {
namespace {

ConnectivityMonitor::State toState(Reachability reachability) noexcept {
    switch (reachability) {
    case Reachability::Online:
        return ConnectivityMonitor::State::Online;
    case Reachability::Offline:
        return ConnectivityMonitor::State::Offline;
    case Reachability::Unknown:
        break;
    }
    return ConnectivityMonitor::State::Unknown;
}

} // namespace

ConnectivityMonitor::ConnectivityMonitor(std::unique_ptr<ReachabilitySource> source, QObject* parent)
    : QObject(parent), source_(std::move(source)) {
    if (source_) {
        connect(source_.get(), &ReachabilitySource::reachabilityChanged, this, &ConnectivityMonitor::update);
        state_ = toState(source_->reachability());
    }
}

std::unique_ptr<ConnectivityMonitor> ConnectivityMonitor::createDefault(QObject* parent) {
    return std::make_unique<ConnectivityMonitor>(std::make_unique<QtNetworkInformationSource>(), parent);
}

ConnectivityMonitor::State ConnectivityMonitor::state() const noexcept {
    return state_;
}

bool ConnectivityMonitor::isOffline() const noexcept {
    return state_ == State::Offline;
}

void ConnectivityMonitor::refresh() {
    update();
}

void ConnectivityMonitor::update() {
    if (!source_) {
        return;
    }
    const State next = toState(source_->reachability());
    if (next == state_) {
        return;
    }
    const bool wasOffline = isOffline();
    state_ = next;
    emit stateChanged();
    if (wasOffline != isOffline()) {
        emit isOfflineChanged();
    }
}

} // namespace aedst::connectivity
