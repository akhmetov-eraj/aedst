#pragma once

#include <functional>
#include <map>
#include <string>
#include <string_view>

namespace aedst::platform {

enum class OperatingSystem {
    Windows,
    MacOs,
    Linux,
    Other,
};

[[nodiscard]] constexpr std::string_view toString(OperatingSystem os) noexcept {
    switch (os) {
    case OperatingSystem::Windows:
        return "windows";
    case OperatingSystem::MacOs:
        return "macos";
    case OperatingSystem::Linux:
        return "linux";
    case OperatingSystem::Other:
        break;
    }
    return "other";
}

/// Environment snapshot. `std::less<>` lets lookups use string_view keys.
using EnvironmentMap = std::map<std::string, std::string, std::less<>>;

/// Raw facts about the running system. Everything PlatformInfo derives comes
/// from here, so tests inject a probe instead of touching the real machine.
struct PlatformProbe {
    OperatingSystem os{OperatingSystem::Other};
    std::string osVersion;
    std::string architecture;
    EnvironmentMap environment;
    /// True when `/.flatpak-info` exists (the documented Flatpak marker).
    bool flatpakInfoExists{false};
};

/// Secrets-free description of the host, safe to log and to attach to
/// diagnostics: no paths, user names, host names or arbitrary env values.
struct PlatformInfo {
    OperatingSystem os{OperatingSystem::Other};
    std::string osVersion;
    std::string architecture;
    /// "wayland" | "x11" | "tty" | "mir" | "win32" | "cocoa" | "unknown".
    std::string displayServer;
    /// Sanitised XDG_CURRENT_DESKTOP (e.g. "ubuntu:GNOME"); "windows"/"aqua" elsewhere.
    std::string desktopEnvironment;
    bool isFlatpak{false};
    /// Flatpak, Snap or the macOS App Sandbox: the app cannot freely touch the
    /// user's home directory or the session bus.
    bool isSandboxed{false};

    friend bool operator==(const PlatformInfo&, const PlatformInfo&) = default;
};

/// Pure: derives PlatformInfo from an injected probe.
[[nodiscard]] PlatformInfo describePlatform(const PlatformProbe& probe);

/// Reads the real machine. Only a fixed allow-list of environment variables is
/// copied into the probe (never the whole environment).
[[nodiscard]] PlatformProbe currentPlatformProbe();

[[nodiscard]] PlatformInfo currentPlatformInfo();

} // namespace aedst::platform
