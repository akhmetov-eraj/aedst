#pragma once

#include "aedst/core/outcome.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace aedst::update {

/// What the client remembers between checks to defeat rollback / replay.
struct UpdateState {
    /// Highest manifest `publishedAt` (Unix seconds) ever accepted; 0 = none yet.
    std::int64_t lastSeenPublishedAt{0};
    /// Highest manifest version ever accepted; empty = none yet.
    std::string lastSeenVersion;

    friend bool operator==(const UpdateState&, const UpdateState&) = default;
};

class UpdateStateStore {
public:
    UpdateStateStore() = default;
    UpdateStateStore(const UpdateStateStore&) = delete;
    UpdateStateStore& operator=(const UpdateStateStore&) = delete;
    UpdateStateStore(UpdateStateStore&&) = delete;
    UpdateStateStore& operator=(UpdateStateStore&&) = delete;
    virtual ~UpdateStateStore() = default;

    /// A missing state is not an error: it returns the default (first run).
    [[nodiscard]] virtual core::Outcome<UpdateState> load() const = 0;
    [[nodiscard]] virtual core::VoidOutcome save(const UpdateState& state) = 0;
};

/// Two `key=value` lines, replaced atomically (temp file + rename). A corrupt
/// file reads as the default state (and is overwritten on the next save).
class FileUpdateStateStore final : public UpdateStateStore {
public:
    explicit FileUpdateStateStore(std::filesystem::path file);

    [[nodiscard]] core::Outcome<UpdateState> load() const override;
    [[nodiscard]] core::VoidOutcome save(const UpdateState& state) override;

private:
    std::filesystem::path file_;
};

} // namespace aedst::update
