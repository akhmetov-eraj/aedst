# AEDST Desktop

Qt 6 / QML desktop client for Windows and Linux (macOS builds, packaging is a
scope cut). Same "monolith first, boundaries first" module architecture as
`apps/android` and `apps/apple` — see
[`docs/content/architecture-desktop.md`](../../docs/content/architecture-desktop.md) for how
the server's modules map onto CMake targets and why features never link each
other.

> **Not built in the authoring environment.** This tree was written without a
> Qt/Conan toolchain available, to be internally consistent (target graph,
> include paths, Qt 6.8 CMake API, Conan 2 recipe names). Run the Getting
> started steps below and fix anything the first real configure/compile flags.

## Stack

| Concern | Choice |
|---|---|
| Language | C++23 (`std::expected`, designated initializers) |
| UI | Qt 6.8+ Qt Quick / QML, Qt Quick Controls (Basic style) |
| Architecture | Module Architecture (Core libraries + Feature libraries + App composition root); MVVM + Clean Architecture (Domain / Data / Presentation / Navigation) per feature |
| Build | CMake ≥ 3.25 with `CMakePresets.json` (`debug`, `release`), Ninja |
| Packages | Conan 2 (`conanfile.py`) for everything except Qt |
| Qt itself | Official Qt online installer or [aqtinstall](https://github.com/miurahr/aqtinstall); found via `CMAKE_PREFIX_PATH` / `$QT_ROOT_DIR` |
| Networking | Qt Network (`QNetworkAccessManager`) speaking ConnectRPC's JSON protocol; Boost.Asio optional for future low-level I/O |
| Database | SQLite (Conan `sqlite3`) behind a thin RAII wrapper (`AedstPersistence`) |
| Crypto / secrets | Windows DPAPI; OpenSSL 3 AES-256-GCM elsewhere; OpenSSL SHA-256 for token fingerprints |
| Logging | spdlog (`AedstLogger`: console + rotating file) for app code; Qt `QLoggingCategory` reserved for Qt/QML framework messages |
| Tests | GoogleTest/gMock (pure C++) + Qt Test (QObject/ViewModels), all via CTest |
| Static analysis | clang-tidy (`.clang-tidy`), clang-format (`.clang-format`), PVS-Studio (local, licensed) |
| Docs | Doxygen (`Doxyfile` → `docs/generated/`) |
| IDE | Qt Creator (opens `CMakeLists.txt` / `CMakePresets.json` directly — no `.pro` files) |
| Packaging | Windows: NSIS (CPack) + MSIX (unsigned); Linux: Flatpak, AppImage, DEB/TGZ (CPack) |

## Layout

```
Modules/Core/{Common,Network,Persistence,Navigation,Analytics,DesignSystem,Testing}
                                  -> Aedst::Core, Aedst::Network, ... (never depend on a feature)
Modules/Features/{Auth,Users,Profile}
                                  -> Aedst::Feature::Auth, ... (depend on Core only)
App/                              -> aedst-desktop (the only target linking every feature)
Tests/                            -> label-scoped `check*` targets; tests live beside each module
cmake/, packaging/                -> warnings, CPack, NSIS/MSIX/Flatpak/AppImage
```

## Getting started

Prerequisites: a C++23 compiler (MSVC 17.10+, GCC 13+, Clang 17+), CMake ≥ 3.25,
Ninja, Python 3 + `pip install conan` (2.x), and Qt 6.8+:

```bash
# Qt via aqtinstall (or the official online installer)
pip install aqtinstall
aqt install-qt linux desktop 6.8.3 linux_gcc_64 -O ~/Qt        # Windows: windows desktop 6.8.3 win64_msvc2022_64
export QT_ROOT_DIR=~/Qt/6.8.3/gcc_64                              # PowerShell: $env:QT_ROOT_DIR="C:\Qt\6.8.3\msvc2022_64"

conan profile detect --force                                       # once per machine
```

Debug build + tests (run from `apps/desktop`; on Windows, from a
"x64 Native Tools" prompt so Ninja finds MSVC):

```bash
conan install . --output-folder=build --build=missing -s build_type=Debug
cmake --preset debug
cmake --build --preset debug
ctest --preset debug                 # or: debug-unit / debug-qttest
```

Release:

```bash
conan install . --output-folder=build --build=missing -s build_type=Release
cmake --preset release
cmake --build --preset release
```

`conan install` writes `build/<preset>/generators/conan_toolchain.cmake`, which
is exactly the `toolchainFile` each preset points at. Add `-o with_asio=True`
to make Boost.Asio available (see `Modules/Core/Network/include/aedst/network/asio_support.hpp`).

Run it (the API defaults to `http://localhost:8080`, i.e. `apps/server` running locally):

```bash
AEDST_API_BASE_URL=http://localhost:8080 ./build/debug/App/aedst-desktop
```

On Windows, put `%QT_ROOT_DIR%\bin` on `PATH` (or run `windeployqt`) before
launching from the build tree or running `ctest`.

## Qt Creator

File → Open File or Project → `apps/desktop/CMakeLists.txt`. Qt Creator reads
`CMakePresets.json` and offers the `debug` / `release` presets as kits — run the
matching `conan install` first so the toolchain file exists. Set the kit's Qt
version to your 6.8 install. There are no `.pro`/qmake files.

## Tests

- **GoogleTest / gMock** — Domain use cases, repositories, DTO decoding,
  SQLite/crypto (`Modules/**/tests/*_test.cpp`, CTest label `unit`).
- **Qt Test** — QObject ViewModels with `QSignalSpy`
  (`Modules/Features/Auth/tests/login_view_model_test.cpp`, label `qttest`), run
  with `QT_QPA_PLATFORM=offscreen`.
- Convenience targets: `cmake --build --preset debug --target check-unit` /
  `check-qttest` / `check`.

## Static analysis

```bash
# formatting (CI runs this with --dry-run --Werror)
find Modules App -name '*.hpp' -o -name '*.cpp' | xargs clang-format -i

# clang-tidy against the configured preset's compile_commands.json
run-clang-tidy -p build/debug -quiet 'Modules/|App/'
# or build with it inline: cmake --preset debug -DAEDST_ENABLE_CLANG_TIDY=ON
```

**PVS-Studio** needs a license (free for open source / students, trial
otherwise), so it isn't wired into CI. Local usage against the same
compilation database:

```bash
pvs-studio-analyzer credentials <name> <key>                     # once
pvs-studio-analyzer analyze -f build/debug/compile_commands.json \
    -e build/ -j8 -o build/pvs.log
plog-converter -a GA:1,2 -t fullhtml build/pvs.log -o build/pvs-report
```

(Windows: `CompilerCommandsAnalyzer.exe analyze --cfg ...` with the same
`compile_commands.json`, or the Visual Studio plugin.)

## Documentation

```bash
doxygen Doxyfile        # -> docs/generated/html/index.html (gitignored)
```

## Packaging

From a configured **release** tree:

| Target / command | Output | Notes |
|---|---|---|
| `cpack --config build/release/CPackConfig.cmake` | NSIS `.exe` + `.zip` (Windows), `.deb` + `.tar.gz` (Linux) | CI path |
| `cmake --build --preset release --target package_nsis` | NSIS from `packaging/windows/installer.nsi.in` | needs `makensis` |
| `cmake --build --preset release --target package_msix` | unsigned `.msix` | needs Windows SDK `makeappx`/`makepri`; signing is a scope cut |
| `cmake --build --preset release --target package_appimage` | `.AppImage` | configure with `-DAEDST_DEPLOY_QT=OFF`; needs `linuxdeploy` + Qt plugin and a real icon |
| `cmake --build --preset release --target package_flatpak` | `.flatpak` bundle | needs `flatpak-builder`; builds against `org.kde.Platform` 6.8 |

Artifacts land in `build/<preset>/packages/`. Icons are placeholders — see
`App/resources/icons/README.md`.
