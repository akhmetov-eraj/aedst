#pragma once

#include <string>
#include <string_view>

namespace aedst::core {

/// Fresh random 64-bit correlation id as 16 lowercase hex characters. Seeded
/// per thread from std::random_device; not a security token, only a log/trace key.
[[nodiscard]] std::string newCorrelationId();

/// True for 1..64 characters of [A-Za-z0-9._-]. This is the only format that is
/// ever placed in a log line or an `X-Correlation-Id` header, so it cannot carry
/// CR/LF (header/log injection) or unbounded input.
[[nodiscard]] bool isValidCorrelationId(std::string_view id) noexcept;

/// The calling thread's active correlation id, or an empty view outside any
/// CorrelationScope. The view is valid until the scope on this thread changes.
[[nodiscard]] std::string_view currentCorrelationId() noexcept;

/// RAII: makes `id` the calling thread's correlation id for its lifetime (printed
/// by the `%C` pattern flag / the JSON "cid" field) and restores the previous one
/// on destruction, so scopes nest. Thread-local: it does not follow work handed
/// to another thread or to a later event-loop turn - create a scope there too.
class CorrelationScope final {
public:
    /// Generates a new id.
    CorrelationScope();
    /// Adopts `id`; an invalid id (see isValidCorrelationId) is replaced by a fresh one.
    explicit CorrelationScope(std::string id);
    ~CorrelationScope();

    CorrelationScope(const CorrelationScope&) = delete;
    CorrelationScope& operator=(const CorrelationScope&) = delete;
    CorrelationScope(CorrelationScope&&) = delete;
    CorrelationScope& operator=(CorrelationScope&&) = delete;

    [[nodiscard]] const std::string& id() const noexcept { return id_; }

private:
    std::string id_;
    std::string previous_;
};

} // namespace aedst::core
