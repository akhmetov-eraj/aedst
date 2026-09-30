#include "aedst/crash/consent_store.hpp"

#include "file_util.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>

namespace aedst::crash {
namespace {

constexpr std::size_t kMaxConsentFileBytes = 64;

std::string trimmed(std::string text) {
    const auto isSpace = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (!text.empty() && isSpace(text.back())) {
        text.pop_back();
    }
    std::size_t start = 0;
    while (start < text.size() && isSpace(text[start])) {
        ++start;
    }
    return text.substr(start);
}

} // namespace

FileConsentStore::FileConsentStore(std::filesystem::path file) : file_(std::move(file)) {}

ConsentState FileConsentStore::state() const {
    auto content = detail::readFile(file_, kMaxConsentFileBytes);
    if (!content) {
        return ConsentState::Unknown;
    }
    const std::string value = trimmed(std::move(*content));
    if (value == "granted") {
        return ConsentState::Granted;
    }
    if (value == "denied") {
        return ConsentState::Denied;
    }
    return ConsentState::Unknown;
}

core::VoidOutcome FileConsentStore::setState(ConsentState newState) {
    switch (newState) {
    case ConsentState::Granted:
        return detail::writeFileAtomically(file_, "granted\n");
    case ConsentState::Denied:
        return detail::writeFileAtomically(file_, "denied\n");
    case ConsentState::Unknown:
        break;
    }
    std::error_code ec;
    std::filesystem::remove(file_, ec);
    if (ec) {
        return core::fail(core::Error::storage("cannot clear consent: " + ec.message()));
    }
    return core::ok();
}

} // namespace aedst::crash
