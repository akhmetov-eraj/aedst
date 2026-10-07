#include "aedst/update/startup_health_guard.hpp"

#include "aedst/update/semver.hpp"

#include <algorithm>
#include <fstream>
#include <ios>
#include <string>
#include <system_error>
#include <utility>

namespace aedst::update {
namespace {

constexpr std::uintmax_t kMaxStateBytes = 1024;
constexpr int kMaxCounter = 1000;

bool parseSmallInt(const std::string& text, int& out) {
    if (text.empty() || text.size() > 6) {
        return false;
    }
    int value = 0;
    for (const char c : text) {
        if (c < '0' || c > '9') {
            return false;
        }
        value = value * 10 + (c - '0');
    }
    out = std::min(value, kMaxCounter);
    return true;
}

} // namespace

StartupHealthGuard::StartupHealthGuard(std::filesystem::path file, std::string currentVersion)
    : file_(std::move(file)), currentVersion_(std::move(currentVersion)) {}

StartupHealthGuard::State StartupHealthGuard::load() const {
    std::error_code ec;
    const std::uintmax_t size = std::filesystem::file_size(file_, ec);
    if (ec || size == 0 || size > kMaxStateBytes) {
        return {};
    }
    std::ifstream in(file_, std::ios::binary);
    if (!in) {
        return {};
    }
    State parsed;
    bool sawStarts = false;
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
            line.pop_back();
        }
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) {
            return {};
        }
        const std::string key = line.substr(0, eq);
        const std::string value = line.substr(eq + 1);
        if (key == "version") {
            if (!value.empty() && !parseSemVer(value)) {
                return {};
            }
            parsed.version = value;
        } else if (key == "unhealthyStarts") {
            if (!parseSmallInt(value, parsed.unhealthyStarts)) {
                return {};
            }
            sawStarts = true;
        } else if (key == "lastKnownGood") {
            if (!value.empty() && !parseSemVer(value)) {
                return {};
            }
            parsed.lastKnownGood = value;
        }
    }
    return sawStarts ? parsed : State{};
}

core::VoidOutcome StartupHealthGuard::save(const State& state) const {
    std::error_code ec;
    if (file_.has_parent_path()) {
        std::filesystem::create_directories(file_.parent_path(), ec);
        if (ec) {
            return core::fail(core::Error::storage("update health: cannot create directory: " + ec.message()));
        }
    }
    std::filesystem::path temp = file_;
    temp += ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) {
            return core::fail(core::Error::storage("update health: cannot open temp file"));
        }
        out << "version=" << state.version << "\nunhealthyStarts=" << state.unhealthyStarts
            << "\nlastKnownGood=" << state.lastKnownGood << "\n";
        out.flush();
        if (!out) {
            return core::fail(core::Error::storage("update health: cannot write temp file"));
        }
    }
    std::filesystem::rename(temp, file_, ec);
    if (ec) {
        std::error_code ignored;
        std::filesystem::remove(temp, ignored);
        return core::fail(core::Error::storage("update health: cannot replace file: " + ec.message()));
    }
    return core::ok();
}

core::Outcome<HealthReport> StartupHealthGuard::recordStart() {
    State state = load();
    const bool firstStart = state.version != currentVersion_;
    if (firstStart) {
        // A new version gets a clean count; what the previous one proved (lastKnownGood) is kept.
        state.version = currentVersion_;
        state.unhealthyStarts = 0;
    }
    state.unhealthyStarts = std::min(state.unhealthyStarts + 1, kMaxCounter);

    HealthReport report{.unhealthyStarts = state.unhealthyStarts,
                        .crashLoop = state.unhealthyStarts >= kCrashLoopThreshold,
                        .firstStartOfThisVersion = firstStart,
                        .lastKnownGood = state.lastKnownGood};
    if (auto saved = save(state); !saved) {
        return core::fail(saved.error());
    }
    return report;
}

core::VoidOutcome StartupHealthGuard::markHealthy() {
    State state = load();
    state.version = currentVersion_;
    state.unhealthyStarts = 0;
    state.lastKnownGood = currentVersion_;
    return save(state);
}

} // namespace aedst::update
