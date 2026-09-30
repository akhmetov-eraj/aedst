#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace aedst::crash {

/// A crash artefact waiting on disk (`crash-*.dmp` or `crash-*.txt`).
struct CrashReport {
    std::filesystem::path path;
    std::uintmax_t sizeBytes{0};
    /// File name without directory; stable identifier for the UI.
    std::string id;

    friend bool operator==(const CrashReport&, const CrashReport&) = default;
};

/// Non-empty `crash-*.dmp` / `crash-*.txt` regular files in `crashesDirectory`,
/// oldest first (names embed a UTC timestamp). Empty files (the unused
/// pre-opened report of a session that did not crash) and unrelated files are
/// ignored. A missing directory yields an empty list.
[[nodiscard]] std::vector<CrashReport> listPendingReports(const std::filesystem::path& crashesDirectory);

/// Replaces the user-name segment of `/home/<name>/`, `/Users/<name>/` and
/// `\Users\<name>\` with `<user>`. Applied to text reports before upload (module
/// paths in backtraces can contain the account name). Binary dumps cannot be
/// scrubbed, which is why they are consent-gated.
[[nodiscard]] std::string scrubHomePaths(std::string_view text);

} // namespace aedst::crash
