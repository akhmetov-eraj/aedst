#include "aedst/telemetry/sentry_reporter.hpp"

#include "aedst/core/logger.hpp"

#include <atomic>
#include <string>

#if defined(AEDST_WITH_SENTRY)
#include <sentry.h>
#endif

namespace aedst::telemetry {
namespace {

using core::AedstLogger;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::atomic<bool> gActive{false};

#if defined(AEDST_WITH_SENTRY)

[[nodiscard]] const char* levelName(BreadcrumbLevel level) noexcept {
    switch (level) {
    case BreadcrumbLevel::Debug:
        return "debug";
    case BreadcrumbLevel::Warning:
        return "warning";
    case BreadcrumbLevel::Error:
        return "error";
    case BreadcrumbLevel::Info:
        break;
    }
    return "info";
}

[[nodiscard]] std::string toString(std::string_view text) {
    return std::string(text);
}

#endif

} // namespace

bool SentryReporter::isCompiledIn() noexcept {
#if defined(AEDST_WITH_SENTRY)
    return true;
#else
    return false;
#endif
}

bool SentryReporter::start(const SentryConfig& config, bool consentGranted) {
#if defined(AEDST_WITH_SENTRY)
    if (config.dsn.empty() || gActive.load()) {
        return gActive.load();
    }
    sentry_options_t* options = sentry_options_new();
    sentry_options_set_dsn(options, config.dsn.c_str());
    sentry_options_set_release(options, config.release.c_str());
    sentry_options_set_environment(options, config.environment.c_str());
    sentry_options_set_debug(options, config.debug ? 1 : 0);
    // Nothing is sent before an explicit "yes" (persisted by sentry in its database
    // and mirrored by Aedst's ConsentStore).
    sentry_options_set_require_user_consent(options, 1);
    sentry_options_set_max_breadcrumbs(options, 100);
#if defined(_WIN32)
    sentry_options_set_database_pathw(options, config.databasePath.wstring().c_str());
    if (!config.handlerPath.empty()) {
        sentry_options_set_handler_pathw(options, config.handlerPath.wstring().c_str());
    }
#else
    sentry_options_set_database_path(options, config.databasePath.string().c_str());
    if (!config.handlerPath.empty()) {
        sentry_options_set_handler_path(options, config.handlerPath.string().c_str());
    }
#endif
    if (sentry_init(options) != 0) { // sentry_init takes ownership of options either way
        AedstLogger::warn("Sentry: sentry_init failed; crash reporting stays local-only");
        return false;
    }
    gActive.store(true);
    setConsent(consentGranted);
    AedstLogger::info("Sentry: initialized (release={}, consent={})", config.release, consentGranted);
    return true;
#else
    (void)config;
    (void)consentGranted;
    return false;
#endif
}

bool SentryReporter::isActive() noexcept {
    return gActive.load();
}

void SentryReporter::setConsent(bool granted) {
#if defined(AEDST_WITH_SENTRY)
    if (!gActive.load()) {
        return;
    }
    if (granted) {
        sentry_user_consent_give();
    } else {
        sentry_user_consent_revoke();
    }
#else
    (void)granted;
#endif
}

void SentryReporter::setUser(std::string_view userId) {
#if defined(AEDST_WITH_SENTRY)
    if (!gActive.load() || userId.empty()) {
        return;
    }
    // id only: no email / username / ip_address keys, by contract (see header).
    sentry_value_t user = sentry_value_new_object();
    sentry_value_set_by_key(user, "id", sentry_value_new_string(toString(userId).c_str()));
    sentry_set_user(user);
#else
    (void)userId;
#endif
}

void SentryReporter::clearUser() {
#if defined(AEDST_WITH_SENTRY)
    if (gActive.load()) {
        sentry_remove_user();
    }
#endif
}

void SentryReporter::addBreadcrumb(std::string_view category, std::string_view message, BreadcrumbLevel level) {
#if defined(AEDST_WITH_SENTRY)
    if (!gActive.load()) {
        return;
    }
    sentry_value_t crumb = sentry_value_new_breadcrumb("default", toString(message).c_str());
    sentry_value_set_by_key(crumb, "category", sentry_value_new_string(toString(category).c_str()));
    sentry_value_set_by_key(crumb, "level", sentry_value_new_string(levelName(level)));
    sentry_add_breadcrumb(crumb);
#else
    (void)category;
    (void)message;
    (void)level;
#endif
}

void SentryReporter::captureError(std::string_view context, std::string_view message) {
#if defined(AEDST_WITH_SENTRY)
    if (!gActive.load()) {
        return;
    }
    sentry_capture_event(
        sentry_value_new_message_event(SENTRY_LEVEL_ERROR, toString(context).c_str(), toString(message).c_str()));
#else
    (void)context;
    (void)message;
#endif
}

void SentryReporter::shutdown() {
#if defined(AEDST_WITH_SENTRY)
    if (gActive.exchange(false)) {
        sentry_close();
    }
#endif
}

} // namespace aedst::telemetry
