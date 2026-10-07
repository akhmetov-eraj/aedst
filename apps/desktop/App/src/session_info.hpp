#pragma once

#include <QObject>
#include <QString>

namespace aedst::app {

/// App-level view of "who is signed in" for the shell (Main.qml): whether a
/// session exists and whether its access token carries the admin role. Lives in
/// App/ because it combines Auth's session with the Admin feature's entry point -
/// no feature may know both. Registered as the `Session` singleton of `Aedst.App`.
class SessionInfo final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool signedIn READ signedIn NOTIFY changed)
    Q_PROPERTY(bool isAdmin READ isAdmin NOTIFY changed)

public:
    explicit SessionInfo(QObject* parent = nullptr) : QObject(parent) {}

    [[nodiscard]] bool signedIn() const noexcept { return signedIn_; }
    [[nodiscard]] bool isAdmin() const noexcept { return isAdmin_; }

    void set(bool signedIn, bool isAdmin) {
        if (signedIn == signedIn_ && isAdmin == isAdmin_) {
            return;
        }
        signedIn_ = signedIn;
        isAdmin_ = signedIn && isAdmin;
        emit changed();
    }

signals:
    void changed();

private:
    bool signedIn_{false};
    bool isAdmin_{false};
};

} // namespace aedst::app
