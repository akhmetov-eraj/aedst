#pragma once

#include "aedst/persistence/master_key_provider.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

namespace aedst::persistence::test_support {

/// Test double: fixed key derived from one fill byte, no I/O. Two providers with
/// different fill bytes model "wrong key".
class InMemoryMasterKeyProvider final : public MasterKeyProvider {
public:
    explicit InMemoryMasterKeyProvider(std::uint8_t fill = 0x42) noexcept : fill_(fill) {}

    [[nodiscard]] std::string_view name() const noexcept override { return "in-memory"; }

    [[nodiscard]] core::Outcome<SecretBytes> loadOrCreate() override {
        ++loadCount;
        return SecretBytes{std::vector<std::uint8_t>(kMasterKeySize, fill_)};
    }

    int loadCount{0};

private:
    std::uint8_t fill_;
};

} // namespace aedst::persistence::test_support
