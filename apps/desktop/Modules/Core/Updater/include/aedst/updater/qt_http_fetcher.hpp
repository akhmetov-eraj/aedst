#pragma once

#include "aedst/update/http_fetcher.hpp"

#include <chrono>

namespace aedst::updater {

/// update::HttpFetcher over Qt Network. Blocking by contract, so it must be called from a worker thread: it reuses one
/// thread-local QNetworkAccessManager per calling thread (connection reuse) and runs a local event loop per fetch.
///
/// Policy: https only (anything else is refused before a socket is opened), no redirects followed (a signed manifest
/// is fetched from the exact URL that was configured), TLS verification stays on (no ignoreSslErrors), the body is
/// aborted as soon as it exceeds `maxBytes`, and the whole request has a deadline.
class QtHttpFetcher final : public update::HttpFetcher {
public:
    explicit QtHttpFetcher(std::chrono::seconds timeout = std::chrono::seconds{30}) noexcept : timeout_(timeout) {}

    [[nodiscard]] core::Outcome<std::string> fetch(std::string_view url, std::size_t maxBytes) override;

private:
    std::chrono::seconds timeout_;
};

} // namespace aedst::updater
