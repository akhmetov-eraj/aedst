#include "aedst/platform/platform_info.hpp"

#include <QByteArray>
#include <QString>
#include <QSysInfo>
#include <QtGlobal>

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

namespace aedst::platform {
namespace {

constexpr std::size_t kMaxDesktopLength = 64;

constexpr std::array<const char*, 5> kAllowedEnvironmentKeys{
    "XDG_SESSION_TYPE", "XDG_CURRENT_DESKTOP", "WAYLAND_DISPLAY", "DISPLAY", "SNAP",
};

char asciiLower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return static_cast<char>(c - 'A' + 'a');
    }
    return c;
}

std::string lowered(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        out.push_back(asciiLower(c));
    }
    return out;
}

std::string_view lookup(const EnvironmentMap& env, std::string_view key) {
    const auto it = env.find(key);
    if (it == env.end()) {
        return {};
    }
    return it->second;
}

bool isDesktopChar(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == ':' || c == '.' ||
           c == '_' || c == '-';
}

std::string sanitizeDesktop(std::string_view raw) {
    if (raw.empty() || raw.size() > kMaxDesktopLength) {
        return {};
    }
    for (const char c : raw) {
        if (!isDesktopChar(c)) {
            return {};
        }
    }
    return std::string{raw};
}

std::string detectDisplayServer(const PlatformProbe& probe) {
    switch (probe.os) {
    case OperatingSystem::Windows:
        return "win32";
    case OperatingSystem::Linux:
        break;
    case OperatingSystem::Other:
        return "unknown";
    }
    const std::string sessionType = lowered(lookup(probe.environment, "XDG_SESSION_TYPE"));
    if (sessionType == "wayland" || sessionType == "x11" || sessionType == "tty" || sessionType == "mir") {
        return sessionType;
    }
    if (!lookup(probe.environment, "WAYLAND_DISPLAY").empty()) {
        return "wayland";
    }
    if (!lookup(probe.environment, "DISPLAY").empty()) {
        return "x11";
    }
    return "unknown";
}

std::string detectDesktop(const PlatformProbe& probe) {
    switch (probe.os) {
    case OperatingSystem::Windows:
        return "windows";
    case OperatingSystem::Linux:
        return sanitizeDesktop(lookup(probe.environment, "XDG_CURRENT_DESKTOP"));
    case OperatingSystem::Other:
        break;
    }
    return {};
}

} // namespace

PlatformInfo describePlatform(const PlatformProbe& probe) {
    PlatformInfo info;
    info.os = probe.os;
    info.osVersion = probe.osVersion;
    info.architecture = probe.architecture;
    info.displayServer = detectDisplayServer(probe);
    info.desktopEnvironment = detectDesktop(probe);
    info.isFlatpak = probe.os == OperatingSystem::Linux && probe.flatpakInfoExists;
    const bool snap = probe.os == OperatingSystem::Linux && !lookup(probe.environment, "SNAP").empty();
    info.isSandboxed = info.isFlatpak || snap;
    return info;
}

PlatformProbe currentPlatformProbe() {
    PlatformProbe probe;
#if defined(_WIN32)
    probe.os = OperatingSystem::Windows;
#elif defined(__linux__)
    probe.os = OperatingSystem::Linux;
#else
    probe.os = OperatingSystem::Other;
#endif
    probe.osVersion = QSysInfo::productVersion().toStdString();
    probe.architecture = QSysInfo::currentCpuArchitecture().toStdString();
    for (const char* key : kAllowedEnvironmentKeys) {
        if (qEnvironmentVariableIsSet(key)) {
            probe.environment.emplace(key, qEnvironmentVariable(key).toStdString());
        }
    }
    std::error_code ec;
    probe.flatpakInfoExists = std::filesystem::exists("/.flatpak-info", ec) && !ec;
    return probe;
}

PlatformInfo currentPlatformInfo() {
    return describePlatform(currentPlatformProbe());
}

} // namespace aedst::platform
