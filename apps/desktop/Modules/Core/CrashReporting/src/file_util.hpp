#pragma once

// Private to AedstCrashReporting: tiny std::filesystem/iostream helpers.

#include "aedst/core/outcome.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>

namespace aedst::crash::detail {

/// Reads a whole file (binary). Fails if it is larger than `maxBytes`.
[[nodiscard]] inline core::Outcome<std::string> readFile(const std::filesystem::path& path, std::uintmax_t maxBytes) {
    std::error_code ec;
    const std::uintmax_t size = std::filesystem::file_size(path, ec);
    if (ec) {
        return core::fail(core::Error::storage("cannot stat file: " + ec.message()));
    }
    if (size > maxBytes) {
        return core::fail(core::Error::invalidArgument("file too large"));
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return core::fail(core::Error::storage("cannot open file"));
    }
    std::string content{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    return content;
}

/// Writes `content` to a sibling temp file and renames it over `path`.
[[nodiscard]] inline core::VoidOutcome writeFileAtomically(const std::filesystem::path& path,
                                                           std::string_view content) {
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            return core::fail(core::Error::storage("cannot create directory: " + ec.message()));
        }
    }
    std::filesystem::path temp = path;
    temp += ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) {
            return core::fail(core::Error::storage("cannot open temp file"));
        }
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        out.flush();
        if (!out) {
            return core::fail(core::Error::storage("cannot write temp file"));
        }
    }
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        std::error_code ignored;
        std::filesystem::remove(temp, ignored);
        return core::fail(core::Error::storage("cannot replace file: " + ec.message()));
    }
    return core::ok();
}

} // namespace aedst::crash::detail
