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
  QuickControls2, Network, LinguistTools, Test/QuickTest (tests), DBus (Linux,
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

| Option | Default | Meaning |
|---|---|---|
| `AEDST_SANITIZER` | `none` | `asan-ubsan` or `tsan` (not with LTO) |
| `AEDST_WARNINGS_AS_ERRORS` | `OFF` (ON in CI) | `-Werror` / `/WX` |
| `AEDST_ENABLE_LIBSECRET` | `ON` (Linux) | Secret Service master-key backend when libsecret is found |
| `AEDST_UPDATE_PUBLIC_KEYS` | empty | `keyId:hex64[,keyId:hex64]` Ed25519 keys; empty = updates rejected |
| `AEDST_ENABLE_ASIO` | `OFF` | Boost.Asio for low-level I/O (needs a documented reason) |
| `AEDST_DEPLOY_QT` | `ON` | bundle Qt in the install tree (`OFF` for Flatpak) |
| `AEDST_COVERAGE` | `OFF` | gcov instrumentation (`coverage` preset) |

Runtime environment: `AEDST_API_BASE_URL` (required in Release, https),
`AEDST_ALLOW_INSECURE_LOCALHOST=1` (permit http to loopback outside Debug).
`aedst-desktop --version` prints the version and exits without a display.

## Release builds and packaging

See [packaging/README.md](packaging/README.md): artifacts, signing secrets,
debug-symbol artifacts (`aedst-symbols-<os>`), and install/uninstall verification.
Release configuration refuses `QT_QML_DEBUG`.
