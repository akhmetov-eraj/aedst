#include "Presentation/notifications_view_model.hpp"

// NotificationsViewModel: reachability handling (split out of notifications_view_model.cpp to keep it under the
// file-size limit).

namespace aedst::feature::notifications {

void NotificationsViewModel::setOnline(bool online) {
    if (online == online_) {
        return;
    }
    online_ = online;
    if (!running_) {
        return;
    }
    if (!online_) {
        pollTimer_.stop(); // an in-flight poll finishes (and fails) on its own; none is scheduled while offline
        return;
    }
    if (!(realtime_ && realtime_->isStreaming())) {
        pollNow(); // back online: catch up immediately, then the usual schedule resumes
    }
}

} // namespace aedst::feature::notifications
