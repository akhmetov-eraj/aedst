#include "aedst/crash/crash_report.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace aedst::crash {
namespace {

bool isReportName(const std::string& name) {
    const std::string_view view{name};
    if (!view.starts_with("crash-")) {
        return false;
    }
    return view.ends_with(".dmp") || view.ends_with(".txt");
}

bool isPathBoundary(char c) {
    return c == '/' || c == '\\' || c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '"' || c == '\'';
}

} // namespace

std::vector<CrashReport> listPendingReports(const std::filesystem::path& crashesDirectory) {
    std::vector<CrashReport> reports;
    std::error_code ec;
    std::filesystem::directory_iterator it(crashesDirectory, ec);
    if (ec) {
        return reports;
    }
    for (const std::filesystem::directory_iterator end; it != end; it.increment(ec)) {
        if (ec) {
            break;
        }
        std::error_code entryError;
        if (!it->is_regular_file(entryError) || entryError) {
            continue;
        }
        std::string name = it->path().filename().string();
        if (!isReportName(name)) {
            continue;
        }
        const std::uintmax_t size = it->file_size(entryError);
        if (entryError || size == 0) {
            continue;
        }
        reports.push_back(CrashReport{it->path(), size, std::move(name)});
    }
    std::sort(reports.begin(), reports.end(), [](const CrashReport& a, const CrashReport& b) { return a.id < b.id; });
    return reports;
}

std::string scrubHomePaths(std::string_view text) {
    constexpr std::array<std::string_view, 3> kPrefixes{"/home/", "/Users/", "\\Users\\"};
    std::string out;
    out.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        bool matched = false;
        for (const std::string_view prefix : kPrefixes) {
            if (text.substr(i).starts_with(prefix)) {
                out.append(prefix);
                i += prefix.size();
                while (i < text.size() && !isPathBoundary(text[i])) {
                    ++i;
                }
                out.append("<user>");
                matched = true;
                break;
            }
        }
        if (!matched) {
            out.push_back(text[i]);
            ++i;
        }
    }
    return out;
}

} // namespace aedst::crash
