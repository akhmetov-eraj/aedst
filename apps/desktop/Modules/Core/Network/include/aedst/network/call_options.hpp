#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>

namespace aedst::network {

/// Exponential backoff with "equal jitter" (half fixed, half random), so a fleet
/// of clients that lost the server together does not retry in lockstep.
/// Applied only to idempotent methods and retryable errors (ApiError::isRetryable).
struct RetryPolicy {
    /// Total attempts including the first; 1 disables retries.
    int maxAttempts{3};
    std::chrono::milliseconds initialBackoff{200};
    std::chrono::milliseconds maxBackoff{2000};
    double multiplier{2.0};

    [[nodiscard]] static constexpr RetryPolicy none() noexcept { return RetryPolicy{.maxAttempts = 1}; }

    /// Delay before attempt number `nextAttempt` (2 = first retry). `jitter` is a
    /// uniform sample in [0, 1); injected so the schedule is deterministic in tests.
    [[nodiscard]] std::chrono::milliseconds delayBefore(int nextAttempt, double jitter) const noexcept {
        const int exponent = std::max(nextAttempt - 2, 0);
        const double capMs = std::min(static_cast<double>(maxBackoff.count()),
                                      static_cast<double>(initialBackoff.count()) * std::pow(multiplier, exponent));
        const double unit = std::clamp(jitter, 0.0, 1.0);
        return std::chrono::milliseconds{static_cast<std::int64_t>(capMs * (0.5 + 0.5 * unit))};
    }
};

/// Per-call overrides; anything unset falls back to ClientConfig.
struct CallOptions {
    /// Deadline for each attempt (also sent as `Connect-Timeout-Ms`). 0 = none.
    std::optional<std::chrono::milliseconds> timeout;
    /// Ignored for methods that are not Idempotency::Idempotent.
    std::optional<RetryPolicy> retry;
};

} // namespace aedst::network
