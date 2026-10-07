# Platform support — AEDST Desktop

Targets: **Windows and Linux only** ([ADR-0023](docs/adr/0023-windows-and-linux-only.md)).
macOS is served by the native app in [`apps/apple`](../apple); this tree refuses to
configure on macOS.

Qt is pinned in [`qt-version.txt`](qt-version.txt) (single source of truth). All
status below is **design + CI configuration; nothing has been built yet.**

## Minimum supported versions

| | Windows | Linux |
|---|---|---|
| Operating system | Windows 10 22H2 (build 19045) and later, Windows 11 (all supported feature releases). Qt 6.12 itself needs Windows 10 1809+; 22H2 is the product floor because earlier Windows 10 releases are out of servicing. | Distributions with glibc ≥ 2.39 (Qt's official binaries are built on Ubuntu 24.04), GL/EGL and xkbcommon, Wayland or X11. |
| Distributions we test and support | — | Ubuntu 24.04 LTS and newer, Debian 13 and newer, Fedora (current and previous release). **UNVERIFIED:** cross-check against the "Supported Platforms" page of Qt 6.12 before the first release; CI currently runs only on `ubuntu-latest`. |
| Sandboxed delivery | MSIX (optional) | Flatpak on `org.kde.Platform` 6.12 (runtime not yet on Flathub — see packaging), AppImage (manual) |
| Architectures | x86-64 | x86-64 (arm64 not built) |
| Toolchain | MSVC 19.37+ (VS 2022 17.7) | GCC 13+, Clang 17+ with libstdc++ 13+ |
| Presets | `msvc-debug/release`, `msvc-asan` | `gcc-*`, `clang-*`, `*-asan`, `clang-tsan`, `coverage` |
| Secret store | DPAPI-wrapped master key | Secret Service (libsecret, optional) → `0600` key file fallback |
| Notifications | Native WinRT toast with an Open button (needs the Start Menu shortcut with the AUMID), else tray balloon, else in-app | `org.freedesktop.Notifications` via QtDBus with a click action, else tray, else in-app |
| Autostart | `HKCU\...\Run` | XDG autostart; Flatpak portal documented only |
| Installer | NSIS `.exe`, `.zip`, MSIX (unsigned unless configured) | `.deb`, `.rpm`, `.tar.gz`, Flatpak, AppImage (manual) |
| Signing | Authenticode via `signtool` (needs secrets) | detached GPG `.asc` (needs secret) |
| CI runner | `windows-latest` | `ubuntu-latest` |

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

- AppImage/Flatpak are not built in CI (need real icons, linuxdeploy,
  flatpak-builder).
- Toast click after the app exited (needs the installer's COM activator or protocol registration), portal autostart.
