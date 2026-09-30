#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

class QLockFile;
class QLocalServer;
class QLocalSocket;

namespace aedst::platform {

/// Keeps a single AEDST process per user and forwards a second launch to it.
///
/// Mechanism (all Qt, identical on every OS, so no per-OS split is needed):
///  - QLockFile in `lockDirectory` is the source of truth for "who is primary".
///    QLockFile itself removes locks whose owner PID is gone (crash); the
///    time-based staleness is disabled (a healthy primary runs for days).
///  - QLocalServer/QLocalSocket (named pipe on Windows, Unix socket elsewhere,
///    user-only access) is the activation channel. The secondary sends its
///    command-line arguments as one JSON line and exits; the primary emits
///    activationRequested() so the app can raise its window and handle deep links.
///
/// Arguments crossing the channel are untrusted: they are bounded and filtered
/// here, and the receiver must still validate them (e.g. parseDeepLink()).
class SingleInstanceGuard final : public QObject {
    Q_OBJECT

public:
    enum class Role {
        /// This process owns the lock and serves the activation channel.
        Primary,
        /// Another instance owns the lock; arguments were delivered. Exit now.
        Secondary,
        /// Guard could not be established (no writable dir, owner unreachable,
        /// ...). Fail open: the app decides whether to continue.
        Unavailable,
    };
    Q_ENUM(Role)

    /// `lockDirectory` should be the per-user app data directory
    /// (QStandardPaths::AppLocalDataLocation); it is created if missing.
    SingleInstanceGuard(QString appId, QString lockDirectory, QObject* parent = nullptr);
    ~SingleInstanceGuard() override;

    /// Call once, before the main window exists. Blocks briefly (<~2 s) only
    /// when another instance is running.
    [[nodiscard]] Role acquire(const QStringList& arguments);

    [[nodiscard]] Role role() const noexcept { return role_; }

    /// Filters an untrusted argument list: max 16 entries, each 1..2048 chars and
    /// free of control characters. Exposed for tests.
    [[nodiscard]] static QStringList sanitizeArguments(const QStringList& arguments);

signals:
    /// Emitted on the primary when a second launch happened. Raise the window.
    void activationRequested(const QStringList& arguments);

private:
    [[nodiscard]] Role becomePrimary();
    [[nodiscard]] bool sendActivation(const QStringList& arguments) const;
    [[nodiscard]] bool lockLooksAbandoned() const;
    void onNewConnection();
    void handleReadable(QLocalSocket* socket);

    QString appId_;
    QString lockDirectory_;
    QString serverName_;
    Role role_{Role::Unavailable};
    std::unique_ptr<QLockFile> lock_;
    std::unique_ptr<QLocalServer> server_;
};

} // namespace aedst::platform
