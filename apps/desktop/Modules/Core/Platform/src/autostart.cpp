#include "aedst/platform/autostart.hpp"

#include <QByteArray>
#include <QSaveFile>
#include <QString>
#include <QtGlobal>
#if defined(_WIN32)
#include <QSettings>
#endif

#include <array>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace aedst::platform {
namespace {

using core::Error;
using core::fail;

constexpr std::size_t kMaxAppIdLength = 128;

std::string utf8String(const std::filesystem::path& path) {
    const auto u8 = path.u8string();
    return std::string{reinterpret_cast<const char*>(u8.data()), u8.size()};
}

QString toQString(const std::filesystem::path& path) {
    return QString::fromStdU16String(path.u16string());
}

bool hasControlChars(std::string_view text) {
    for (const char c : text) {
        const auto u = static_cast<unsigned char>(c);
        if (u < 0x20U || u == 0x7FU) {
            return true;
        }
    }
    return false;
}

/// Neutral to the host OS so tests behave identically everywhere: a leading '/'
/// or a native absolute path both count.
bool isAbsoluteLike(std::string_view text) {
    return !text.empty() && (text.front() == '/' || std::filesystem::path{text}.is_absolute());
}

std::string_view envValue(const EnvironmentMap& env, std::string_view key) {
    const auto it = env.find(key);
    if (it == env.end()) {
        return {};
    }
    return it->second;
}

core::Outcome<std::string> validatedExecutable(const AutostartConfig& config) {
    if (!config.executablePath.is_absolute()) {
        return fail(Error::invalidArgument("autostart: executable path must be absolute"));
    }
    std::string exe = utf8String(config.executablePath);
    if (hasControlChars(exe) || exe.find('"') != std::string::npos) {
        return fail(Error::invalidArgument("autostart: executable path contains forbidden characters"));
    }
    return exe;
}

core::VoidOutcome validateArguments(const AutostartConfig& config) {
    for (const std::string& arg : config.arguments) {
        if (hasControlChars(arg)) {
            return fail(Error::invalidArgument("autostart: argument contains control characters"));
        }
    }
    if (hasControlChars(config.displayName)) {
        return fail(Error::invalidArgument("autostart: display name contains control characters"));
    }
    return core::ok();
}

/// One Exec argument per the Desktop Entry spec: double-quoted, with `"`, `` ` ``,
/// `$` and `\` backslash-escaped; `%` doubled (field codes); then the string-value
/// rule doubles every backslash again ("applied before the quoting rule").
std::string quoteDesktopExecArg(std::string_view arg) {
    std::string quoted{"\""};
    for (const char c : arg) {
        if (c == '"' || c == '`' || c == '$' || c == '\\') {
            quoted.push_back('\\');
            quoted.push_back(c);
        } else if (c == '%') {
            quoted += "%%";
        } else {
            quoted.push_back(c);
        }
    }
    quoted.push_back('"');
    std::string out;
    out.reserve(quoted.size());
    for (const char c : quoted) {
        out.push_back(c);
        if (c == '\\') {
            out.push_back('\\');
        }
    }
    return out;
}

std::string xmlEscape(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        switch (c) {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        case '\'':
            out += "&apos;";
            break;
        default:
            out.push_back(c);
            break;
        }
    }
    return out;
}

std::string windowsQuoteIfNeeded(std::string_view arg) {
    if (arg.find_first_of(" \t") == std::string_view::npos) {
        return std::string{arg};
    }
    return "\"" + std::string{arg} + "\"";
}

core::VoidOutcome writeFileAtomically(const std::filesystem::path& path, std::string_view content) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        return fail(Error::storage("autostart: cannot create directory: " + ec.message()));
    }
    QSaveFile file{toQString(path)};
    if (!file.open(QIODevice::WriteOnly)) {
        return fail(Error::storage("autostart: cannot open file for writing"));
    }
    const auto size = static_cast<qint64>(content.size());
    if (file.write(content.data(), size) != size) {
        file.cancelWriting();
        return fail(Error::storage("autostart: short write"));
    }
    if (!file.commit()) {
        return fail(Error::storage("autostart: cannot commit file"));
    }
    return core::ok();
}

core::VoidOutcome removeIfPresent(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
    if (ec) {
        return fail(Error::storage("autostart: cannot remove entry: " + ec.message()));
    }
    return core::ok();
}

bool fileExists(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec) && !ec;
}

} // namespace

bool isValidAutostartId(std::string_view appId) {
    if (appId.empty() || appId.size() > kMaxAppIdLength || appId.front() == '.') {
        return false;
    }
    for (const char c : appId) {
        const bool alnum = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        if (!alnum && c != '.' && c != '-' && c != '_') {
            return false;
        }
    }
    return true;
}

core::Outcome<std::filesystem::path> xdgAutostartFilePath(const EnvironmentMap& env, std::string_view appId) {
    if (!isValidAutostartId(appId)) {
        return fail(Error::invalidArgument("autostart: invalid app id"));
    }
    std::filesystem::path base;
    const std::string_view xdg = envValue(env, "XDG_CONFIG_HOME");
    if (isAbsoluteLike(xdg)) {
        base = std::filesystem::path{xdg};
    } else {
        const std::string_view home = envValue(env, "HOME");
        if (!isAbsoluteLike(home)) {
            return fail(Error::notFound("autostart: neither XDG_CONFIG_HOME nor HOME is usable"));
        }
        base = std::filesystem::path{home} / ".config";
    }
    return base / "autostart" / (std::string{appId} + ".desktop");
}

core::Outcome<std::string> renderDesktopEntry(const AutostartConfig& config) {
    auto exe = validatedExecutable(config);
    if (!exe) {
        return fail(exe.error());
    }
    if (auto args = validateArguments(config); !args) {
        return fail(args.error());
    }
    std::string exec = quoteDesktopExecArg(*exe);
    for (const std::string& arg : config.arguments) {
        exec.push_back(' ');
        exec += quoteDesktopExecArg(arg);
    }
    std::string out;
    out += "[Desktop Entry]\n";
    out += "Type=Application\n";
    out += "Version=1.5\n";
    out += "Name=" + config.displayName + "\n";
    out += "Exec=" + exec + "\n";
    out += "Terminal=false\n";
    out += "X-GNOME-Autostart-enabled=true\n";
    return out;
}

core::Outcome<std::string> renderWindowsRunCommand(const AutostartConfig& config) {
    AutostartConfig native = config;
    native.executablePath.make_preferred();
    auto exe = validatedExecutable(native);
    if (!exe) {
        return fail(exe.error());
    }
    if (auto args = validateArguments(native); !args) {
        return fail(args.error());
    }
    std::string out = "\"" + *exe + "\"";
    for (const std::string& arg : native.arguments) {
        if (arg.find('"') != std::string::npos) {
            return fail(Error::invalidArgument("autostart: argument contains a quote"));
        }
        out.push_back(' ');
        out += windowsQuoteIfNeeded(arg);
    }
    return out;
}

// ---- XdgAutostartManager ---------------------------------------------------

XdgAutostartManager::XdgAutostartManager(AutostartConfig config, std::filesystem::path filePath)
    : config_(std::move(config)), filePath_(std::move(filePath)) {}

bool XdgAutostartManager::isAvailable() const {
    return !filePath_.empty() && isValidAutostartId(config_.appId);
}

bool XdgAutostartManager::isEnabled() const {
    return isAvailable() && fileExists(filePath_);
}

core::VoidOutcome XdgAutostartManager::setEnabled(bool enabled) {
    if (!isAvailable()) {
        return fail(Error::invalidArgument("autostart: not available"));
    }
    if (!enabled) {
        return removeIfPresent(filePath_);
    }
    auto content = renderDesktopEntry(config_);
    if (!content) {
        return fail(content.error());
    }
    return writeFileAtomically(filePath_, *content);
}

// ---- WindowsRunKeyAutostartManager -----------------------------------------

#if defined(_WIN32)
namespace {
const QString kRunKeyPath = QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
} // namespace

WindowsRunKeyAutostartManager::WindowsRunKeyAutostartManager(AutostartConfig config) : config_(std::move(config)) {}

bool WindowsRunKeyAutostartManager::isAvailable() const {
    const QSettings settings(kRunKeyPath, QSettings::NativeFormat);
    return isValidAutostartId(config_.appId) && settings.status() == QSettings::NoError;
}

bool WindowsRunKeyAutostartManager::isEnabled() const {
    const QSettings settings(kRunKeyPath, QSettings::NativeFormat);
    return settings.status() == QSettings::NoError && settings.contains(QString::fromStdString(config_.appId));
}

core::VoidOutcome WindowsRunKeyAutostartManager::setEnabled(bool enabled) {
    if (!isValidAutostartId(config_.appId)) {
        return fail(Error::invalidArgument("autostart: invalid app id"));
    }
    QSettings settings(kRunKeyPath, QSettings::NativeFormat);
    const QString name = QString::fromStdString(config_.appId);
    if (enabled) {
        auto command = renderWindowsRunCommand(config_);
        if (!command) {
            return fail(command.error());
        }
        settings.setValue(name, QString::fromStdString(*command));
    } else {
        settings.remove(name);
    }
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        return fail(Error::storage("autostart: registry write failed"));
    }
    return core::ok();
}
#endif

// ---- UnsupportedAutostartManager -------------------------------------------

UnsupportedAutostartManager::UnsupportedAutostartManager(std::string reason) : reason_(std::move(reason)) {}

bool UnsupportedAutostartManager::isAvailable() const {
    return false;
}

bool UnsupportedAutostartManager::isEnabled() const {
    return false;
}

core::VoidOutcome UnsupportedAutostartManager::setEnabled(bool /*enabled*/) {
    return fail(Error::unknown("autostart unsupported: " + reason_));
}

// ---- Factory ---------------------------------------------------------------

std::unique_ptr<AutostartManager> makeAutostartManager(AutostartConfig config, const PlatformProbe& probe) {
    if (!isValidAutostartId(config.appId)) {
        return std::make_unique<UnsupportedAutostartManager>("invalid app id");
    }
#if defined(_WIN32)
    static_cast<void>(probe);
    return std::make_unique<WindowsRunKeyAutostartManager>(std::move(config));
#else
    if (probe.flatpakInfoExists) {
        return std::make_unique<UnsupportedAutostartManager>(
            "sandboxed (Flatpak): needs the org.freedesktop.portal.Background portal");
    }
    auto path = xdgAutostartFilePath(probe.environment, config.appId);
    if (!path) {
        return std::make_unique<UnsupportedAutostartManager>(path.error().message);
    }
    return std::make_unique<XdgAutostartManager>(std::move(config), std::move(*path));
#endif
}

std::unique_ptr<AutostartManager> makeAutostartManager(AutostartConfig config) {
    PlatformProbe probe = currentPlatformProbe();
    constexpr std::array<const char*, 2> kPathKeys{"HOME", "XDG_CONFIG_HOME"};
    for (const char* key : kPathKeys) {
        if (qEnvironmentVariableIsSet(key)) {
            probe.environment.emplace(key, qEnvironmentVariable(key).toStdString());
        }
    }
    return makeAutostartManager(std::move(config), probe);
}

} // namespace aedst::platform
