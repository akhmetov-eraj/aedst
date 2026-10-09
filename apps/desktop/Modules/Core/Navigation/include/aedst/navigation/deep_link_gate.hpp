#pragma once

#include "aedst/navigation/router.hpp"

#include <QString>

#include <optional>
#include <utility>

namespace aedst::navigation {

/// Holds a deep link that arrives before the user is signed in and opens it once they are.
///
/// Without it a cold-start `aedst://tasks/<id>` was pushed onto the login screen and then wiped by the
/// `resetTo(taskList)` that follows sign-in. Router::openLink still validates every link against the active routes,
/// so a gate never widens the allow-list. Only the most recent pending link is kept.
class DeepLinkGate final {
public:
    explicit DeepLinkGate(Router& router) noexcept : router_(router) {}

    /// Opens `link` now when signed in, otherwise remembers it. Returns true only when it was opened.
    bool submit(const QString& link) {
        if (!signedIn_) {
            pending_ = link;
            return false;
        }
        return router_.openLink(link);
    }

    /// Call AFTER the signed-in home screen is on the stack: a pending link is then pushed on top of it.
    void setSignedIn(bool signedIn) {
        signedIn_ = signedIn;
        if (!signedIn_) {
            pending_.reset(); // a link kept from one session must never open in another
            return;
        }
        if (auto link = std::exchange(pending_, std::nullopt)) {
            router_.openLink(*link);
        }
    }

    [[nodiscard]] bool hasPending() const noexcept { return pending_.has_value(); }

private:
    Router& router_;
    std::optional<QString> pending_;
    bool signedIn_{false};
};

} // namespace aedst::navigation
