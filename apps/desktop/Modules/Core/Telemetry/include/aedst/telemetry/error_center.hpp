#pragma once

#include "aedst/core/error_sink.hpp"

#include <QElapsedTimer>
#include <QObject>
#include <QString>

#include <string>

namespace aedst::telemetry {

/// The app's single typed-error -> UI path (the desktop analogue of a global
/// toast/error boundary). Features report core::Error through the core::ErrorSink
/// port; this class
///  1. logs it (AedstLogger) and records a crash-reporter breadcrumb,
///  2. maps the code to a stable UI kind string that QML turns into a localized
///     title (App/qml/Main.qml's toast host / banner): "network", "unauthenticated",
///     "permission_denied", "not_found", "invalid_argument", "storage", "unknown",
///  3. emits errorRaised() unless the visibility is Silent, collapsing identical
///     errors within kDedupWindowMs so a failing poll cannot stack toasts,
///  4. emits sessionExpired() for Unauthenticated so App/ can route to login once.
/// Cancelled errors are dropped (user intent, not a failure). Unknown errors are
/// additionally captured as a Sentry event (they mean a bug or a contract gap).
/// GUI thread only (it is a QObject used from QML).
class ErrorCenter final : public QObject, public core::ErrorSink {
    Q_OBJECT

public:
    static constexpr qint64 kDedupWindowMs = 3000;

    explicit ErrorCenter(QObject* parent = nullptr);

    void report(const core::Error& error, std::string_view context,
                core::ErrorVisibility visibility = core::ErrorVisibility::Transient) override;

    /// Stable kind string for a code (exposed for QML tests and the mapping test).
    [[nodiscard]] static QString kindFor(core::ErrorCode code);
    [[nodiscard]] static bool isRetryable(core::ErrorCode code) noexcept;

signals:
    /// `blocking` = show a persistent banner instead of an auto-dismissing toast.
    void errorRaised(const QString& kind, const QString& message, bool retryable, bool blocking);
    void sessionExpired();

private:
    std::string lastKey_;
    QElapsedTimer lastAt_;
};

} // namespace aedst::telemetry
