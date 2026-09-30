#include "aedst/platform/single_instance_guard.hpp"

#include "aedst/core/logger.hpp"

#include <QByteArray>
#include <QChar>
#include <QCryptographicHash>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QSysInfo>
#include <QThread>
#include <QTimer>

#if defined(_WIN32)
#include <windows.h>
#endif

#include <utility>

namespace aedst::platform {
namespace {

constexpr qsizetype kMaxArguments = 16;
constexpr qsizetype kMaxArgumentChars = 2048;
constexpr qint64 kMaxMessageBytes = 128 * 1024;
constexpr int kClientReadTimeoutMs = 2000;
constexpr int kConnectAttempts = 5;
constexpr int kConnectTimeoutMs = 300;
constexpr int kRetryDelayMs = 100;
constexpr int kWriteTimeoutMs = 1000;

QString makeServerName(const QString& appId, const QString& lockDirectory) {
    // The name space of pipes/sockets is shared between users on some OSes, so
    // the per-user lock directory is hashed into the name.
    const QByteArray digest =
        QCryptographicHash::hash(QDir::cleanPath(lockDirectory).toUtf8(), QCryptographicHash::Sha256).toHex();
    QString safeId;
    for (const QChar c : appId) {
        safeId.append(c.isLetterOrNumber() ? c : QChar{u'-'});
    }
    return QStringLiteral("aedst-") + safeId.left(40) + QLatin1Char('-') + QString::fromLatin1(digest.left(16));
}

bool hasControlChars(const QString& text) {
    for (const QChar c : text) {
        if (c.unicode() < 0x20 || c.unicode() == 0x7F) {
            return true;
        }
    }
    return false;
}

} // namespace

SingleInstanceGuard::SingleInstanceGuard(QString appId, QString lockDirectory, QObject* parent)
    : QObject(parent),
      appId_(std::move(appId)),
      lockDirectory_(std::move(lockDirectory)),
      serverName_(makeServerName(appId_, lockDirectory_)) {}

SingleInstanceGuard::~SingleInstanceGuard() = default;

QStringList SingleInstanceGuard::sanitizeArguments(const QStringList& arguments) {
    QStringList out;
    for (const QString& argument : arguments) {
        if (out.size() >= kMaxArguments) {
            break;
        }
        if (argument.isEmpty() || argument.size() > kMaxArgumentChars || hasControlChars(argument)) {
            continue;
        }
        out.append(argument);
    }
    return out;
}

SingleInstanceGuard::Role SingleInstanceGuard::acquire(const QStringList& arguments) {
    if (!QDir().mkpath(lockDirectory_)) {
        core::AedstLogger::warn("single instance: cannot create the lock directory");
        role_ = Role::Unavailable;
        return role_;
    }
    lock_ = std::make_unique<QLockFile>(QDir(lockDirectory_).filePath(appId_ + QStringLiteral(".lock")));
    // 0 = never stale by age. Dead owners (same host) are still detected by PID,
    // and tryLock() removes such locks by itself.
    lock_->setStaleLockTime(0);

    if (lock_->tryLock(0)) {
        return becomePrimary();
    }
    if (lock_->error() != QLockFile::LockFailedError) {
        core::AedstLogger::warn("single instance: lock file unusable (permissions or I/O error)");
        lock_.reset();
        role_ = Role::Unavailable;
        return role_;
    }

    if (sendActivation(arguments)) {
        role_ = Role::Secondary;
        return role_;
    }

    // The lock is held but nobody answers. Only reclaim it when it is provably
    // abandoned (unreadable lock file, or written on another host of a roaming
    // profile); a live-but-unreachable owner on this host is left alone.
    if (lockLooksAbandoned() && lock_->removeStaleLockFile() && lock_->tryLock(0)) {
        core::AedstLogger::warn("single instance: reclaimed an abandoned lock");
        return becomePrimary();
    }
    core::AedstLogger::warn("single instance: lock held by an unreachable instance; continuing without the guard");
    lock_.reset();
    role_ = Role::Unavailable;
    return role_;
}

SingleInstanceGuard::Role SingleInstanceGuard::becomePrimary() {
    server_ = std::make_unique<QLocalServer>();
    server_->setSocketOptions(QLocalServer::UserAccessOption);
    // A crashed primary leaves its socket file behind on Unix; no-op on Windows.
    QLocalServer::removeServer(serverName_);
    if (!server_->listen(serverName_)) {
        core::AedstLogger::warn("single instance: activation channel unavailable; second launches cannot raise us");
        server_.reset();
    } else {
        connect(server_.get(), &QLocalServer::newConnection, this, &SingleInstanceGuard::onNewConnection);
    }
    role_ = Role::Primary;
    return role_;
}

bool SingleInstanceGuard::lockLooksAbandoned() const {
    qint64 pid = 0;
    QString hostName;
    QString appName;
    if (!lock_->getLockInfo(&pid, &hostName, &appName)) {
        return true;
    }
    return !hostName.isEmpty() && hostName != QSysInfo::machineHostName();
}

bool SingleInstanceGuard::sendActivation(const QStringList& arguments) const {
    QByteArray payload =
        QJsonDocument(QJsonArray::fromStringList(sanitizeArguments(arguments))).toJson(QJsonDocument::Compact);
    if (payload.size() >= kMaxMessageBytes) {
        payload = QByteArrayLiteral("[]");
    }
    payload.append('\n');

#if defined(_WIN32)
    // Lets the primary bring its window to the foreground on our behalf.
    AllowSetForegroundWindow(ASFW_ANY);
#endif

    for (int attempt = 0; attempt < kConnectAttempts; ++attempt) {
        QLocalSocket socket;
        socket.connectToServer(serverName_);
        if (socket.waitForConnected(kConnectTimeoutMs)) {
            socket.write(payload);
            const bool written = socket.waitForBytesWritten(kWriteTimeoutMs);
            socket.disconnectFromServer();
            if (socket.state() != QLocalSocket::UnconnectedState) {
                socket.waitForDisconnected(kWriteTimeoutMs);
            }
            return written;
        }
        // The primary takes the lock a moment before it starts listening.
        QThread::msleep(kRetryDelayMs);
    }
    return false;
}

void SingleInstanceGuard::onNewConnection() {
    while (server_ != nullptr && server_->hasPendingConnections()) {
        QLocalSocket* socket = server_->nextPendingConnection();
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QLocalSocket::readyRead, this, [this, socket]() { handleReadable(socket); });
        // Drop clients that connect and then stall.
        QTimer::singleShot(kClientReadTimeoutMs, socket, [socket]() { socket->abort(); });
    }
}

void SingleInstanceGuard::handleReadable(QLocalSocket* socket) {
    if (socket->bytesAvailable() > kMaxMessageBytes) {
        socket->abort();
        return;
    }
    if (!socket->canReadLine()) {
        return;
    }
    // One message per connection: ignore anything that follows.
    socket->disconnect(this);
    const QByteArray line = socket->readLine(kMaxMessageBytes + 1);
    socket->disconnectFromServer();

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        return;
    }
    QStringList arguments;
    for (const QJsonValue& value : document.array()) {
        if (!value.isString()) {
            return;
        }
        arguments.append(value.toString());
    }
    emit activationRequested(sanitizeArguments(arguments));
}

} // namespace aedst::platform
