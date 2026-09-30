#pragma once

#include "aedst/core/outcome.hpp"

#include <filesystem>

namespace aedst::crash {

enum class ConsentState {
    /// The user was never asked (or the stored value is unreadable): do not upload.
    Unknown,
    Granted,
    Denied,
};

/// Persistence of the user's crash-upload decision. Kept behind an interface so
/// the app can later back it with its settings store; the default is a file.
class ConsentStore {
public:
    ConsentStore() = default;
    ConsentStore(const ConsentStore&) = delete;
    ConsentStore& operator=(const ConsentStore&) = delete;
    ConsentStore(ConsentStore&&) = delete;
    ConsentStore& operator=(ConsentStore&&) = delete;
    virtual ~ConsentStore() = default;

    [[nodiscard]] virtual ConsentState state() const = 0;
    /// Unknown clears the stored decision (the UI will ask again).
    [[nodiscard]] virtual core::VoidOutcome setState(ConsentState newState) = 0;
};

/// One-word text file ("granted" / "denied"), replaced atomically (temp file +
/// rename). Anything else on disk reads as Unknown.
class FileConsentStore final : public ConsentStore {
public:
    explicit FileConsentStore(std::filesystem::path file);

    [[nodiscard]] ConsentState state() const override;
    [[nodiscard]] core::VoidOutcome setState(ConsentState newState) override;

private:
    std::filesystem::path file_;
};

} // namespace aedst::crash
