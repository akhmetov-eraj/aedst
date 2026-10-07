# ADR-0023: Desktop targets Windows and Linux only

- Status: accepted (not compiled in the authoring environment)
- Scope: `apps/desktop` (CMake, presets, CI, packaging, `Aedst::Platform`, `Aedst::Persistence`)
- Supersedes in part: ADR-0003 (Keychain provider removed), ADR-0006 (LaunchAgent autostart removed), ADR-0022 (Qt 6.12 macOS floor note)

## Context

The Qt client exists for native performance, heavy media, large local data, low
memory use and deep OS integration. macOS is already served by the native
SwiftUI app in `apps/apple`. Keeping a third macOS client in `apps/desktop` meant
a second macOS code path (Keychain, LaunchAgent, bundle/DMG/PKG packaging,
notarization, an AppleClang CI job) with no owner, no runner that has ever
executed it, and a Qt 6.12 deployment floor (macOS 14.4) that is higher than the
Apple client's own.

## Decision

- `apps/desktop` supports **Windows 10 22H2+ / Windows 11** and **Linux**
  (matrix and distributions: `apps/desktop/PLATFORM_SUPPORT.md`).
- macOS is `apps/apple`. The root `CMakeLists.txt` fails fast with that message
  when configured on macOS.
- Removed: `macos-*` configure/build/test/workflow presets, the `macos-*` CI
  jobs and Apple signing secrets, `cmake/packaging/macOS*.cmake`,
  `MacOSPreBuild.cmake`, `packaging/macos/`, `verify-install-macos.sh`,
  `keychain_backend_apple.cpp`, the LaunchAgent autostart manager,
  `OperatingSystem::MacOs` and the `APP_SANDBOX_CONTAINER_ID` probe.
- `AppleClang` is no longer a supported compiler; sanitizer and warning
  CMake modules list only MSVC, GCC and Clang.

## Consequences

- Fewer platform branches; every remaining `#if` is Windows vs. everything else.
- The Linux distribution list is a support statement, not a tested matrix: CI
  runs on `ubuntu-latest` only. Qt 6.12's own supported-platform list has not
  been cross-checked (marked UNVERIFIED in `PLATFORM_SUPPORT.md`).
- Windows 10 support depends on Qt 6.12, the last release that supports it.

## Revisit when

A product requirement needs Qt on macOS that `apps/apple` cannot serve, or a
macOS owner and runner exist.
