#include "aedst/telemetry/error_center.hpp"

#include "aedst/core/logger.hpp"
#include "aedst/core/redaction.hpp"
#include "aedst/telemetry/sentry_reporter.hpp"

#include <string>

namespace aedst::telemetry {

using core::AedstLogger;
using core::ErrorCode;

ErrorCenter::ErrorCenter(QObject* parent) : QObject(parent) {}

QString ErrorCenter::kindFor(ErrorCode code) {
    const std::string_view name = core::toString(code);
    return QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size()));
}

bool ErrorCenter::isRetryable(ErrorCode code) noexcept {
    return code == ErrorCode::Network || code == ErrorCode::Unknown || code == ErrorCode::Storage;
}

void ErrorCenter::report(const core::Error& error, std::string_view context, core::ErrorVisibility visibility) {
    if (error.code == ErrorCode::Cancelled) {
        return;
    }
    const std::string kind(core::toString(error.code));
    const std::string message = core::redactSecrets(error.message);
    AedstLogger::warn("{}: {} ({})", context, message, kind);
    SentryReporter::addBreadcrumb(context, kind + ": " + message,
                                  error.code == ErrorCode::Unknown ? BreadcrumbLevel::Error : BreadcrumbLevel::Warning);
    if (error.code == ErrorCode::Unknown) {
        SentryReporter::captureError(context, message);
    }

    if (error.code == ErrorCode::Unauthenticated) {
        emit sessionExpired();
    }
    if (visibility == core::ErrorVisibility::Silent) {
        return;
    }

    std::string key = kind;
    key += '|';
    key += context;
    key += '|';
    key += message;
    if (key == lastKey_ && lastAt_.isValid() && lastAt_.elapsed() < kDedupWindowMs) {
        return;
    }
    lastKey_ = std::move(key);
    lastAt_.start();

    emit errorRaised(kindFor(error.code), QString::fromStdString(message), isRetryable(error.code),
                     visibility == core::ErrorVisibility::Blocking);
}

} // namespace aedst::telemetry
