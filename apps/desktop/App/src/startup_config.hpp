#pragma once

#include "aedst/core/logger.hpp"

#include <QString>
#include <QUrl>

#include <filesystem>

// Process-level startup helpers split out of main.cpp (no behaviour change): argument sniffing,
// per-install ids, endpoint/baked-in configuration and the TLS backend choice.

namespace aedst::app::startup {

/// `--version` / `-v` is answered before any Qt object exists.
[[nodiscard]] bool wantsVersionOnly(int argc, char* argv[]);
/// Stable per-install id sent as LoginRequest.device_id.
[[nodiscard]] QString deviceId();
/// The API endpoint (Debug: local dev server default; Release: none).
[[nodiscard]] QUrl apiBaseUrl();
/// Runtime environment wins over the value baked in at build time.
[[nodiscard]] QString configured(const char* variable, const char* buildDefault);
/// Random per-install id for anonymous analytics (never derived from hardware).
[[nodiscard]] QString analyticsAnonymousId();
/// crashpad_handler is deployed next to the executable.
[[nodiscard]] std::filesystem::path crashpadHandlerPath();
/// TLS backend (ADR-0024). Must run before the first network access.
void selectTlsBackend();

/// Where profile data (database, logs, crash dumps, consent files) lives. `AEDST_DATA_DIR` overrides the OS location
/// and makes the WHOLE profile live under it (data, `cache/`, `config/` settings), so a perf or test run never
/// touches the real profile. Call after the application name is set.
[[nodiscard]] std::filesystem::path dataDirectory();
/// The disk cache root (media thumbnails and previews). Under `AEDST_DATA_DIR/cache` when that is set.
[[nodiscard]] QString cacheRoot();

/// `AEDST_LOG_LEVEL` (trace|debug|info|warn|error|critical|off) or `fallback` when unset or unrecognised.
[[nodiscard]] aedst::core::LogLevel logLevel(aedst::core::LogLevel fallback);

/// `--perf-exit-after-ms=<n>` (perf harness): quit n ms after the first frame. 0 = never. Parsed from argv so it
/// works before any Qt object exists.
[[nodiscard]] int perfExitAfterMs(int argc, char* argv[]);

} // namespace aedst::app::startup
