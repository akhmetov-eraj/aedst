#pragma once

#include "aedst/core/clock.hpp"
#include "aedst/core/outcome.hpp"
#include "aedst/crash/consent_store.hpp"
#include "aedst/crash/crash_report.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace aedst::crash {

struct UploadPayload {
    /// File name only (no directory).
    std::string fileName;
    std::string contentType;
    /// Raw bytes (binary safe).
    std::string body;
};

/// Transport, injected so this module never touches the network. The app layer
/// implements it over ConnectClient / QNetworkAccessManager. Synchronous by
/// contract: call CrashUploader::uploadPending() from a worker thread.
class HttpPoster {
public:
    HttpPoster() = default;
    HttpPoster(const HttpPoster&) = delete;
    HttpPoster& operator=(const HttpPoster&) = delete;
    HttpPoster(HttpPoster&&) = delete;
    HttpPoster& operator=(HttpPoster&&) = delete;
    virtual ~HttpPoster() = default;

    [[nodiscard]] virtual core::VoidOutcome post(const UploadPayload& payload) = 0;
};

struct UploadSummary {
    bool consentGranted{false};
    std::size_t uploaded{0};
    /// Transport or read failures; the report stays on disk for the next attempt.
    std::size_t failed{0};
    /// Reports over the size cap; deleted because they can never be sent.
    std::size_t discardedOversize{0};
    /// Stopped early because the per-day upload budget is used up.
    bool rateLimited{false};

    friend bool operator==(const UploadSummary&, const UploadSummary&) = default;
};

class CrashUploader {
public:
    CrashUploader() = default;
    CrashUploader(const CrashUploader&) = delete;
    CrashUploader& operator=(const CrashUploader&) = delete;
    CrashUploader(CrashUploader&&) = delete;
    CrashUploader& operator=(CrashUploader&&) = delete;
    virtual ~CrashUploader() = default;

    /// What the UI should offer to send ("The app closed unexpectedly...").
    [[nodiscard]] virtual std::vector<CrashReport> pendingReports() const = 0;

    /// Sends pending reports if (and only if) consent is granted.
    [[nodiscard]] virtual core::Outcome<UploadSummary> uploadPending() = 0;

    /// "Don't send": deletes pending reports; returns how many were removed.
    virtual std::size_t discardPending() = 0;
};

struct UploaderConfig {
    /// The `crashes` directory written by CrashHandler.
    std::filesystem::path crashesDirectory;
    std::uintmax_t maxReportBytes{4U * 1024U * 1024U};
    /// Rolling 24 h budget, persisted in `<crashesDirectory>/upload_log.txt` so a
    /// crash loop across restarts cannot flood the endpoint.
    std::size_t maxUploadsPerDay{5};
};

/// Uploads only when ConsentStore says Granted. Text reports are scrubbed of
/// home-directory user names (scrubHomePaths) before sending; each report is
/// deleted after a successful post.
class ConsentGatedCrashUploader final : public CrashUploader {
public:
    /// `consent`, `poster` and `clock` must outlive the uploader.
    ConsentGatedCrashUploader(UploaderConfig config, const ConsentStore& consent, HttpPoster& poster,
                              const core::Clock& clock);

    [[nodiscard]] std::vector<CrashReport> pendingReports() const override;
    [[nodiscard]] core::Outcome<UploadSummary> uploadPending() override;
    std::size_t discardPending() override;

private:
    [[nodiscard]] std::vector<std::int64_t> loadRecentUploads(std::int64_t nowSeconds) const;
    void saveRecentUploads(const std::vector<std::int64_t>& uploads) const;

    UploaderConfig config_;
    const ConsentStore& consent_;
    HttpPoster& poster_;
    const core::Clock& clock_;
};

} // namespace aedst::crash
