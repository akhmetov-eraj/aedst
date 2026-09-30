#include "aedst/core/correlation.hpp"

#include <cstddef>
#include <cstdint>
#include <random>
#include <utility>

namespace aedst::core {
namespace {

constexpr std::size_t kIdHexDigits = 16;
constexpr std::size_t kMaxIdLength = 64;

std::string& activeId() {
    thread_local std::string id;
    return id;
}

std::mt19937_64& generator() {
    // One engine per thread, seeded once from the OS entropy source, so no lock
    // is needed and ids do not repeat across threads.
    thread_local std::mt19937_64 engine = [] {
        std::random_device device;
        std::seed_seq seed{device(), device(), device(), device()};
        return std::mt19937_64(seed);
    }();
    return engine;
}

constexpr bool isIdChar(char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' ||
           c == '-';
}

} // namespace

std::string newCorrelationId() {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::uint64_t value = generator()();
    std::string id(kIdHexDigits, '0');
    for (std::size_t i = kIdHexDigits; i > 0; --i) {
        id[i - 1] = kDigits[value & 0xFU];
        value >>= 4U;
    }
    return id;
}

bool isValidCorrelationId(std::string_view id) noexcept {
    if (id.empty() || id.size() > kMaxIdLength) {
        return false;
    }
    for (const char c : id) {
        if (!isIdChar(c)) {
            return false;
        }
    }
    return true;
}

std::string_view currentCorrelationId() noexcept {
    return activeId();
}

CorrelationScope::CorrelationScope() : CorrelationScope(newCorrelationId()) {}

CorrelationScope::CorrelationScope(std::string id)
    : id_(isValidCorrelationId(id) ? std::move(id) : newCorrelationId()), previous_(activeId()) {
    activeId() = id_;
}

CorrelationScope::~CorrelationScope() {
    activeId() = std::move(previous_);
}

} // namespace aedst::core
