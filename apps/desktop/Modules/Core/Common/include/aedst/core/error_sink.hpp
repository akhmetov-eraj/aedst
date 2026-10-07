#pragma once

#include "aedst/core/error.hpp"

#include <string_view>

namespace aedst::core {

/// How user-visible a reported error should be. The UI layer maps it to a toast
/// (Transient) or a persistent banner (Blocking); Silent errors are only recorded
/// (log + crash-reporter breadcrumb), e.g. a background poll that failed offline.
enum class ErrorVisibility {
    Silent,
    Transient,
    Blocking,
};

/// Qt-free port every feature ViewModel reports failures through, so the app has
/// ONE place that turns a typed error into UI feedback, a log line and a
/// telemetry breadcrumb. App/ injects Aedst::Telemetry's ErrorCenter; tests inject
/// a recording fake. `context` is a short, static, non-PII label such as
/// "tasks.list" (it becomes a Sentry breadcrumb category).
class ErrorSink {
public:
    ErrorSink() = default;
    ErrorSink(const ErrorSink&) = delete;
    ErrorSink& operator=(const ErrorSink&) = delete;
    ErrorSink(ErrorSink&&) = delete;
    ErrorSink& operator=(ErrorSink&&) = delete;
    virtual ~ErrorSink() = default;

    virtual void report(const Error& error, std::string_view context,
                        ErrorVisibility visibility = ErrorVisibility::Transient) = 0;
};

/// Default when nothing is injected (unit tests that do not care).
class NullErrorSink final : public ErrorSink {
public:
    void report(const Error& /*error*/, std::string_view /*context*/, ErrorVisibility /*visibility*/) override {}
};

} // namespace aedst::core
