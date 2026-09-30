# Platform support — AEDST Desktop

Qt is pinned in [`qt-version.txt`](qt-version.txt) (single source of truth). All
status below is **design + CI configuration; nothing has been built yet.**

| Criterion | Windows | macOS | Linux |
|---|---|---|---|
| Minimum OS | Windows 10 (Qt 6.8 requirement) | macOS 12 (`CMAKE_OSX_DEPLOYMENT_TARGET`) | glibc distros with GL/xkbcommon; Flatpak `org.kde.Platform` 6.8 |
| Toolchain | MSVC 19.37+ (VS 2022 17.7) | AppleClang (arm64 + x86_64 presets) | GCC 13+, Clang 17+ with libstdc++ 13+ |
| Presets | `msvc-debug/release`, `msvc-asan` | `macos-debug/release`, `macos-x64-*`, `macos-asan` | `gcc-*`, `clang-*`, `*-asan`, `clang-tsan`, `coverage` |
| Secret store | DPAPI-wrapped master key | Keychain (Security.framework) | Secret Service (libsecret, optional) → `0600` key file fallback |
| Notifications | in-app only (native toast not implemented) | in-app only (`UserNotifications` not implemented) | `org.freedesktop.Notifications` via QtDBus, else in-app |
| Autostart | `HKCU\...\Run` | LaunchAgent plist (`SMAppService` documented only) | XDG autostart; Flatpak portal documented only |
| Installer | NSIS `.exe`, `.zip`, MSIX (unsigned unless configured) | `.dmg`, `.pkg` | `.deb`, `.rpm`, `.tar.gz`, Flatpak, AppImage (manual) |
| Signing | Authenticode via `signtool` (needs secrets) | Developer ID + notarization (needs secrets) | detached GPG `.asc` (needs secret) |
| CI runner | `windows-latest` | `macos-latest` (behaviour of the label: UNVERIFIED) | `ubuntu-latest` |

## Capability checks and fallbacks

Every platform capability has a runtime availability check and a fallback:

- Keyring unreachable → file master key, logged at warning level.
- No notification daemon → in-app `AedstToast` signal.
- No QNetworkInformation backend → connectivity `Unknown` (treated as online).
- Autostart unsupported (Flatpak) → `UnsupportedAutostartManager`, UI must grey
  the toggle via `isAvailable()`.
- Wayland vs X11 and desktop environment are reported by `PlatformInfo`
  (`XDG_SESSION_TYPE`, `XDG_CURRENT_DESKTOP`, Flatpak detection) for diagnostics
  and future workarounds; no per-DE code paths exist yet.

## Known gaps

- No universal macOS binary (build both presets, merge with `lipo`).
- AppImage/Flatpak are not built in CI (need real icons, linuxdeploy,
  flatpak-builder).
- Native Windows/macOS notifications, `SMAppService`, portal autostart.
