#pragma once

#include "aedst/core/logger.hpp"

#include <QElapsedTimer>

#include <string_view>

namespace aedst::app {

/// Startup-time markers: each mark() logs the milliseconds since construction
/// (create it first thing in main()) to the product log, e.g.
///
///     startup +212 ms: dependencies ready
///     startup +388 ms: first frame
///
/// Compare runs by grepping `startup +` in <AppData>/logs/aedst.log; this is the
/// measurement every startup-time change (QML compilation, eager registration,
/// allocator swaps) must quote before/after. Cost is one log line per phase.
class StartupTrace final {
public:
    StartupTrace() noexcept { timer_.start(); }

    void mark(std::string_view phase) const {
        core::AedstLogger::info("startup +{} ms: {}", timer_.elapsed(), phase);
    }

private:
    QElapsedTimer timer_;
};

} // namespace aedst::app
