#pragma once

#include "aedst/core/outcome.hpp"

#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>

namespace aedst::crash {

/// "YYYYMMDDTHHMMSSZ" (UTC) for `when`. Pure; used for crash file names.
[[nodiscard]] std::string formatCrashTimestamp(std::chrono::system_clock::time_point when);

/// "crash-<timestamp>-<pid><extension>", e.g. "crash-20231114T221320Z-4242.txt".
/// `extension` includes the dot.
[[nodiscard]] std::string crashFileName(std::chrono::system_clock::time_point when, unsigned long processId,
                                        std::string_view extension);

/// Local-only crash capture. Nothing is sent anywhere by this class; reports are
/// written under `<directory>/crashes/` and picked up later by a
/// CrashUploader, and only if the user consented.
///
/// - Windows: SetUnhandledExceptionFilter + MiniDumpWriteDump (dbghelp) writes
///   `crash-<utc>-<pid>.dmp`. MiniDumpNormal keeps stacks and the module list but
///   no heap; stacks can still hold user data, so treat dumps as sensitive.
/// - Linux: sigaction (SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL) on an
///   alternate stack. The handler uses only async-signal-safe calls (write,
///   fsync, close, time, raise, sigaction, and backtrace_symbols_fd, after
///   backtrace() was warmed up at install time) and writes a small text report
///   to a file descriptor opened at install time, then re-raises the signal with
///   the default disposition so the OS still produces its own core/crash report.
///   The report holds: app version, signal, epoch time, and raw frames
///   (module path + offset). No environment, arguments, tokens or user data.
///   Platforms without <execinfo.h> (e.g. musl) get the report without frames.
///
/// VERIFICATION STATUS: the signal/dump paths are process-fatal and cannot be
/// unit-tested; they are verified manually (see CrashHandlerManualTest notes in
/// the module CMake) and were NOT compiled or run in the authoring environment.
class CrashHandler final {
public:
    CrashHandler() = delete;

    /// Installs the handlers once per process (later calls are no-ops).
    /// `appVersion` is embedded in POSIX reports; only [A-Za-z0-9._+-] is kept.
    [[nodiscard]] static core::VoidOutcome install(const std::filesystem::path& directory,
                                                   std::string_view appVersion = {});

    /// Restores default handling and removes the unused pre-opened report file
    /// (POSIX). Call on clean shutdown so no empty report is left behind.
    static void uninstall();

    [[nodiscard]] static bool isInstalled();

    /// `<directory>/crashes` as passed to install(); empty before install().
    [[nodiscard]] static std::filesystem::path crashesDirectory();
};

} // namespace aedst::crash
