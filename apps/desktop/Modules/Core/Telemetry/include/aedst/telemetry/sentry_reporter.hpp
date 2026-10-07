#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace aedst::telemetry {

enum class BreadcrumbLevel {
    Debug,
    Info,
    Warning,
    Error,
};

struct SentryConfig {
    /// Empty disables Sentry entirely (local CrashHandler stays in charge).
    std::string dsn;
    /// "aedst-desktop@<version>" - must match the release the CI symbol upload uses
    /// (sentry-cli debug-files upload, see .github/workflows/desktop-ci.yml).
    std::string release;
    std::string environment{"production"};
    /// Crashpad's database (pending minidumps, run state): <AppData>/sentry.
    std::filesystem::path databasePath;
    /// crashpad_handler(.exe) shipped next to the executable. Empty = next to the exe.
    std::filesystem::path handlerPath;
    bool debug{false};
};

/// Thin facade over sentry-native (crashpad backend: out-of-process minidumps on
/// Windows, macOS and Linux). Process-wide, GUI-thread-agnostic (sentry-native is
/// thread-safe). Compiled to no-ops when the build has no sentry-native
/// (-DAEDST_ENABLE_SENTRY=OFF), so callers never #ifdef.
///
/// Privacy contract (template default, keep it):
///  - consent-gated: started with require_user_consent; nothing - crashes included -
///    leaves the machine until setConsent(true). Revoking also drops queued reports;
///  - the only user field ever set is the opaque user id (setUser) - no e-mail,
///    name, IP address or device name; breadcrumbs carry static context labels and
///    error kinds, never request bodies or tokens (AedstLogger redaction applies to
///    anything copied from log text);
///  - release = "aedst-desktop@<version>", environment from the build.
class SentryReporter final {
public:
    SentryReporter() = delete;

    [[nodiscard]] static bool isCompiledIn() noexcept;

    /// Initializes the SDK. Returns false (and stays inert) when the DSN is empty,
    /// the SDK is not compiled in, or sentry_init fails. `consentGranted` is the
    /// stored decision; Unknown counts as not granted.
    [[nodiscard]] static bool start(const SentryConfig& config, bool consentGranted);
    [[nodiscard]] static bool isActive() noexcept;

    static void setConsent(bool granted);
    static void setUser(std::string_view userId);
    static void clearUser();
    static void addBreadcrumb(std::string_view category, std::string_view message, BreadcrumbLevel level);
    /// A handled, non-fatal error worth an event (e.g. an Internal error from the API).
    static void captureError(std::string_view context, std::string_view message);
    /// Flushes and shuts the SDK down (call on clean exit, after the event loop).
    static void shutdown();
};

} // namespace aedst::telemetry
