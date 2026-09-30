#include "aedst/update/update_state_store.hpp"

#include "aedst/update/semver.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>

namespace aedst::update {
namespace {

constexpr std::uintmax_t kMaxStateBytes = 1024;

bool parseInt64(const std::string& text, std::int64_t& out) {
    if (text.empty() || text.size() > 18) {
        return false;
    }
    std::int64_t value = 0;
    for (const char c : text) {
        if (c < '0' || c > '9') {
            return false;
        }
        value = value * 10 + (c - '0');
    }
    out = value;
    return true;
}

} // namespace

FileUpdateStateStore::FileUpdateStateStore(std::filesystem::path file) : file_(std::move(file)) {}

core::Outcome<UpdateState> FileUpdateStateStore::load() const {
    UpdateState state;
    std::error_code ec;
    const std::uintmax_t size = std::filesystem::file_size(file_, ec);
    if (ec || size == 0 || size > kMaxStateBytes) {
        return state;
    }
    std::ifstream in(file_, std::ios::binary);
    if (!in) {
        return state;
    }
    UpdateState parsed;
    bool sawPublishedAt = false;
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
            line.pop_back();
        }
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) {
            return state;
        }
        const std::string key = line.substr(0, eq);
        const std::string value = line.substr(eq + 1);
        if (key == "publishedAt") {
            if (!parseInt64(value, parsed.lastSeenPublishedAt)) {
                return state;
            }
            sawPublishedAt = true;
        } else if (key == "version") {
            if (!value.empty() && !parseSemVer(value)) {
                return state;
            }
            parsed.lastSeenVersion = value;
        }
    }
    if (!sawPublishedAt) {
        return state;
    }
    return parsed;
}

core::VoidOutcome FileUpdateStateStore::save(const UpdateState& state) {
    std::error_code ec;
    if (file_.has_parent_path()) {
        std::filesystem::create_directories(file_.parent_path(), ec);
        if (ec) {
            return core::fail(core::Error::storage("update state: cannot create directory: " + ec.message()));
        }
    }
    std::filesystem::path temp = file_;
    temp += ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) {
            return core::fail(core::Error::storage("update state: cannot open temp file"));
        }
        out << "publishedAt=" << state.lastSeenPublishedAt << "\nversion=" << state.lastSeenVersion << "\n";
        out.flush();
        if (!out) {
            return core::fail(core::Error::storage("update state: cannot write temp file"));
        }
    }
    std::filesystem::rename(temp, file_, ec);
    if (ec) {
        std::error_code ignored;
        std::filesystem::remove(temp, ignored);
        return core::fail(core::Error::storage("update state: cannot replace file: " + ec.message()));
    }
    return core::ok();
}

} // namespace aedst::update
