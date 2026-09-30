#pragma once

#include "aedst/core/outcome.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace aedst::update {

/// HTTPS GET, injected so this module never touches the network. The app layer
/// implements it over ConnectClient / QNetworkAccessManager.
///
/// Contract for implementations:
///  - only https URLs (the caller already validated with isSecureHttpsUrl);
///  - certificate validation must stay on;
///  - abort and fail once the body exceeds `maxBytes`;
///  - synchronous: call UpdateChecker::check() from a worker thread.
class HttpFetcher {
public:
    HttpFetcher() = default;
    HttpFetcher(const HttpFetcher&) = delete;
    HttpFetcher& operator=(const HttpFetcher&) = delete;
    HttpFetcher(HttpFetcher&&) = delete;
    HttpFetcher& operator=(HttpFetcher&&) = delete;
    virtual ~HttpFetcher() = default;

    [[nodiscard]] virtual core::Outcome<std::string> fetch(std::string_view url, std::size_t maxBytes) = 0;
};

} // namespace aedst::update
