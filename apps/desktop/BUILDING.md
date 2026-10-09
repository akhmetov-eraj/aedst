# Building — AEDST Desktop

> **Never built.** The authoring environment had no C++/Qt/CMake toolchain. The
> first configure/compile on your machine is the acceptance test; fix what it
> flags and record recurring surprises here.

## Prerequisites (all platforms)

- CMake ≥ 3.25, Ninja, git, a C++23 compiler (MSVC 19.37+, GCC 13+, Clang 17+
  with libstdc++ 13+).
- [vcpkg](https://github.com/microsoft/vcpkg) with `VCPKG_ROOT` set. Packages come
  from `vcpkg.json` (pinned `builtin-baseline`); Qt does **not** come from vcpkg.
- Qt at the version in `qt-version.txt`, with modules: Core, Gui, Qml, Quick,
  QuickControls2, Network, NetworkAuth (add-on: `qtnetworkauth` with aqtinstall), LinguistTools, Test/QuickTest (tests), DBus (Linux,
  optional). Set `QT_ROOT_DIR`. With aqtinstall add the modules explicitly; if a
  module is missing, configure fails with the missing component name.

### Windows

```powershell
# from an "x64 Native Tools Command Prompt for VS 2022"
$env:VCPKG_ROOT = "C:\vcpkg"; $env:QT_ROOT_DIR = "C:\Qt\6.12.0\msvc2022_64"
cmake --preset msvc-debug; cmake --build --preset msvc-debug; ctest --preset msvc-debug
```
Put `%QT_ROOT_DIR%\bin` on `PATH` (or run `windeployqt`) before running from the build tree.

### Linux

```bash
sudo apt-get install -y build-essential ninja-build pkg-config libgl1-mesa-dev libxkbcommon-dev libegl1 \
    libsecret-1-dev libdbus-1-dev   # libsecret/dbus optional but recommended
export VCPKG_ROOT=~/vcpkg QT_ROOT_DIR=~/Qt/$(cat qt-version.txt)/gcc_64
cmake --preset gcc-debug && cmake --build --preset gcc-debug && ctest --preset gcc-debug
```

## Presets

`debug` / `release` (host compiler), `<msvc|gcc|clang>-<debug|release>`,
`bench`, sanitizers (`gcc-asan`, `clang-asan`, `clang-tsan`, `msvc-asan`),
`coverage`; `wf-*` workflow presets chain configure→build→test.
CI adds `-DAEDST_WARNINGS_AS_ERRORS=ON`.

`scripts/verify-desktop-local.sh <preset>` runs configure + build + test and checks
tool presence with clear messages.

## Options

CMake cache options (`-D<NAME>=...`, or in a preset). Defaults are what a plain `cmake --preset ...` gets.

| Option | Default | Meaning |
|---|---|---|
| `BUILD_TESTING` | `ON` | build the GoogleTest / Qt Test / Qt Quick Test suites and register them with CTest |
| `AEDST_BUILD_BENCHMARKS` | `OFF` | build the `QBENCHMARK` benchmarks and the scroll bench (the `bench` preset; needs `BUILD_TESTING`) |
| `AEDST_WARNINGS_AS_ERRORS` | `OFF` (ON in CI) | `-Werror` / `/WX` |
| `AEDST_ENABLE_CLANG_TIDY` | `OFF` | run clang-tidy inside the build (slow; CI runs it separately) |
| `AEDST_ENABLE_HARDENING` | `ON` | exploit-mitigation flags on every first-party target (`cmake/AedstHardening.cmake`): CFG / `/sdl` / `/DYNAMICBASE` / `/CETCOMPAT` on MSVC; stack protector, stack-clash protection, `-fcf-protection`, PIE, `_FORTIFY_SOURCE=3` (optimised builds), RELRO + `now` + non-exec stack on GCC/Clang. Off only to diagnose a toolchain problem |
| `AEDST_SANITIZER` | `none` | `asan-ubsan` or `tsan` (not with LTO; FORTIFY is skipped under a sanitizer) |
| `AEDST_SANITIZER_NO_RECOVER` | `ON` | abort on the first UBSan finding |
| `AEDST_COVERAGE` | `OFF` | gcov instrumentation (`coverage` preset; excludes sanitizers and LTO) |
| `AEDST_ENABLE_LTO` | `ON` | link-time optimisation for Release (skipped when the toolchain lacks it) |
| `AEDST_FAST_LINKER` | `ON` | mold, else lld (GCC/Clang on Linux) |
| `AEDST_USE_COMPILER_CACHE` | `ON` | sccache / ccache as compiler launcher when found |
| `AEDST_REPRODUCIBLE_BUILD` | `ON` | no timestamps or build paths in the binaries |
| `AEDST_RELEASE_DEBUG_SYMBOLS` | `ON` | emit and split off debug symbols for Release (`<build>/symbols`) |
| `AEDST_ENFORCE_QT_VERSION` | `ON` | fail configure when Qt's major.minor differs from `qt-version.txt` |
| `AEDST_PROTO_ROOT` | `../server/api/proto` | directory with the server's `.proto` files |
| `AEDST_BEHAVIOR_FIXTURE_DIR` | `docs/contract/behavior` | shared cross-platform behaviour fixtures used by the tests |
| `AEDST_ENABLE_LIBSECRET` | `ON` (Linux) | Secret Service master-key backend when libsecret is found |
| `AEDST_SYSTEM_QTKEYCHAIN` | `OFF` | use an installed QtKeychain instead of fetching it (Flatpak, offline builds) |
| `AEDST_ENABLE_SENTRY` | `ON` | link sentry-native (crashpad); inert at run time without a DSN |
| `AEDST_SENTRY_DSN`, `AEDST_SENTRY_ENVIRONMENT` | empty, `production` | Sentry DSN / environment baked in (env of the same name overrides) |
| `AEDST_POSTHOG_KEY`, `AEDST_POSTHOG_HOST` | empty, `https://us.i.posthog.com` | PostHog key / host baked in (https only; env overrides) |
| `AEDST_UPDATE_PUBLIC_KEYS` | empty | `keyId:hex64[,keyId:hex64]` Ed25519 keys; empty = updates rejected |
| `AEDST_UPDATE_MANIFEST_URL`, `AEDST_UPDATE_FEED_URL`, `AEDST_UPDATE_CHANNEL` | empty, empty, `stable` | signed manifest URL, Velopack feed, channel (env overrides) |
| `AEDST_ENABLE_VELOPACK` | `OFF` | build the Velopack installer adapter (self-update for installed builds) |
| `AEDST_ENABLE_RUNTIME_PLUGINS` | `OFF` | build the loader for allow-listed, signed run-time plugins (ADR-0033); OFF = no code path loads a library from disk |
| `AEDST_DEPLOY_QT` | `ON` | bundle Qt in the install tree (`OFF` for Flatpak and AppImage); on Linux it goes to a private `<libdir>/aedst-desktop` |
| `AEDST_MSIX_PUBLISHER`, `AEDST_MSIX_SIGN` | `CN=AEDST Development`, `OFF` | MSIX identity publisher; add a `sign_msix` target |

Runtime environment (all optional):

| Variable | Meaning |
|---|---|
| `AEDST_API_BASE_URL` | API endpoint (https; required in Release) |
| `AEDST_ALLOW_INSECURE_LOCALHOST=1` | permit http to loopback outside Debug |
| `AEDST_DATA_DIR` | relocate the whole profile (data, cache, settings) under this directory; used by the perf harness and tests |
| `AEDST_LOG_LEVEL` | `trace`, `debug`, `info`, `warn`, `error`, `critical`, `off` (default `info` in Release, `debug` in Debug) |
| `AEDST_LOG_RULES` | Qt category rules, e.g. `aedst.network.debug=true` |
| `AEDST_LOG_FORMAT=json` | JSON Lines log file |
| `AEDST_LOG_CONSOLE=1` | Release builds log to the file only; this adds the console |
| `AEDST_OIDC_ISSUER`, `..._CLIENT_ID`, `..._PROJECT_ID`, `..._EXTRA_SCOPES`, `..._LOOPBACK_PORT`, `..._ROLES_SCOPE`, `..._ROLES_CLAIM`, `..._END_SESSION` | OpenID Connect sign-in ([docs/oidc-setup.md](docs/oidc-setup.md)) |
| `AEDST_SENTRY_DSN`, `AEDST_POSTHOG_KEY`, `AEDST_POSTHOG_HOST`, `AEDST_UPDATE_*` | override the values baked in at build time |
| `AEDST_ALLOW_UNSIGNED_PLUGINS=1` | development only: accept unsigned run-time plugins |

Command line: `aedst-desktop --version` prints the version and exits without a display; `--perf-exit-after-ms=<n>` quits `n` ms after the first frame (perf harness).

Hardening (`AEDST_ENABLE_HARDENING`) is verified on the shipped binary by `scripts/verify-install-linux.sh` (readelf: PIE, RELRO, BIND_NOW, non-exec stack, stack protector, RUNPATH) and `scripts/verify-install-windows.ps1` (PE DllCharacteristics: ASLR, high-entropy ASLR, DEP, CFG; winchecksec when installed).

## Release builds and packaging

See [packaging/README.md](packaging/README.md): artifacts, signing secrets,
debug-symbol artifacts (`aedst-symbols-<os>`), and install/uninstall verification.
Release configuration refuses `QT_QML_DEBUG`.
