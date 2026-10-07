# AEDST Desktop

Qt 6 / QML desktop client for Windows, macOS and Linux. Same "monolith first, boundaries first" module architecture as
`apps/android` and `apps/apple` — see
[`docs/content/architecture-desktop.md`](../../docs/content/architecture-desktop.md) for how
the server's modules map onto CMake targets and why features never link each
other.

> **Not built in the authoring environment.** This tree (including the Connect
> client, generated-Protobuf contract, vcpkg migration and performance work) was
> written without a C++/Qt toolchain available. Run the Getting started steps
> below and fix anything the first real configure/compile flags; nothing here
> has been compiled, run, or benchmarked yet.

## Stack

| Concern | Choice |
|---|---|
| Language | C++23 (`std::expected`, designated initializers) |
| UI | Qt 6.12 Qt Quick / QML, Qt Quick Controls (Basic style) |
| Architecture | Module Architecture (Core libraries + Feature libraries + App composition root); MVVM + Clean Architecture (Domain / Data / Presentation / Navigation) per feature |
| Build | CMake ≥ 3.25 with `CMakePresets.json` (`msvc-*`, `gcc-*`, `clang-*`, `macos-*`, sanitizer presets `*-asan`/`clang-tsan`, `coverage`, plus host-default `debug`/`release`/`bench`), Ninja; LTO, sccache/ccache, mold/lld auto-enabled — [performance.md](docs/performance.md) |
| Packages | vcpkg manifest mode (`vcpkg.json`) for everything except Qt — [ADR-0002](docs/adr/0002-dependency-management.md) |
| Qt itself | Official binaries (Qt installer / [aqtinstall](https://github.com/miurahr/aqtinstall)) at the version in `qt-version.txt` (single source of truth; CMake enforces it); found via `$QT_ROOT_DIR` |
| API contract | Generated Protobuf C++ messages (protoc at build time from `apps/server/api/proto`) + a thin Connect-protocol client on Qt Network (unary + server streaming, retries, deadlines, cancellation) — [ADR-0001](docs/adr/0001-api-contract-client.md); Boost.Asio optional for future low-level I/O |
| Database | SQLite (vcpkg `sqlite3`) behind a thin RAII wrapper (`AedstPersistence`) |
| Crypto / secrets | AES-256-GCM (OpenSSL) envelope; master key in DPAPI (Windows) / Keychain (macOS) / Secret Service via libsecret (Linux), `0600` key file only as fallback — [ADR-0003](docs/adr/0003-secret-storage.md) |
| Transport security | https required, TLS ≥ 1.2, optional SPKI pinning — [ADR-0004](docs/adr/0004-transport-policy.md) |
| Platform services | single instance, deep links, autostart, notifications, crash capture, signed-update verification, connectivity — [ADR-0006](docs/adr/0006-platform-services-and-updates.md) |
| Logging | spdlog (`AedstLogger`: console + rotating file) for app code; Qt `QLoggingCategory` reserved for Qt/QML framework messages |
| Tests | GoogleTest/gMock (pure C++, label `unit`) + Qt Test (`qttest`) + Qt Quick Test (`qmltest`), all via CTest; sanitizers and coverage in CI — [TESTING.md](TESTING.md) |
| Static analysis | clang-tidy (blocking in CI with a baseline), clang-format, qmllint, warnings-as-errors in CI, ASan/UBSan/TSan; PVS-Studio (local, licensed) |
| Docs | Doxygen (`Doxyfile` → `docs/generated/`) |
| IDE | Qt Creator (opens `CMakeLists.txt` / `CMakePresets.json` directly — no `.pro` files) |
| Packaging | Windows: NSIS/ZIP (CPack) + MSIX; macOS: DMG + PKG; Linux: DEB/RPM/TGZ (CPack), Flatpak, AppImage — signing when CI secrets exist — [packaging/README.md](packaging/README.md) |

## Layout

```
Modules/Core/{Common,Network,Contract,Persistence,Navigation,Connectivity,Platform,CrashReporting,Update,Analytics,DesignSystem,Testing}
                                  -> Aedst::Core, Aedst::Network, Aedst::Contract, ... (never depend on a feature)
Modules/Features/Auth
                                  -> Aedst::Feature::Auth (depends on Core only; features never link each other)
App/                              -> aedst-desktop (the only target linking every feature)
Tests/                            -> label-scoped `check*` targets; tests live beside each module
cmake/, packaging/                -> warnings, CPack, NSIS/MSIX/Flatpak/AppImage
```

## Getting started

Full instructions per OS: [BUILDING.md](BUILDING.md). Prerequisites: a C++23 compiler (MSVC 19.37+ / VS 2022 17.7, GCC 13+, Clang 17+
with libstdc++ 13+, AppleClang), CMake ≥ 3.25, Ninja, git, [vcpkg](https://github.com/microsoft/vcpkg),
and Qt at the version pinned in `qt-version.txt`. Configure fails early, with a
message, if the compiler or Qt version is wrong.

```bash
# vcpkg (once): dependencies are installed automatically during `cmake --preset`
git clone https://github.com/microsoft/vcpkg ~/vcpkg && ~/vcpkg/bootstrap-vcpkg.sh   # Windows: bootstrap-vcpkg.bat
export VCPKG_ROOT=~/vcpkg                                       # PowerShell: $env:VCPKG_ROOT="C:\vcpkg"

# Qt via aqtinstall (or the official online installer) - version from qt-version.txt
pip install aqtinstall
aqt install-qt linux desktop "$(cat qt-version.txt)" linux_gcc_64 -O ~/Qt   # Windows: windows desktop <ver> win64_msvc2022_64
export QT_ROOT_DIR=~/Qt/$(cat qt-version.txt)/gcc_64                        # PowerShell: $env:QT_ROOT_DIR="C:\Qt\6.12.0\msvc2022_64"
```

Optional but recommended for build speed: `sccache` (or `ccache`), and `mold`
(or `lld`) on Linux — CMake picks them up automatically.

Debug build + tests (run from `apps/desktop`; on Windows from a "x64 Native
Tools" prompt so Ninja finds MSVC):

```bash
cmake --preset gcc-debug          # msvc-debug | clang-debug | debug (host default)
cmake --build --preset gcc-debug
ctest --preset gcc-debug          # or: debug-unit / debug-qttest (host-default preset)
```

The first configure builds protobuf + abseil (minutes); vcpkg's binary cache
(`VCPKG_BINARY_SOURCES`) makes later ones instant. Release: `cmake --preset
gcc-release && cmake --build --preset gcc-release` (LTO on, tests off).
Benchmarks: `cmake --preset bench && cmake --build --preset bench && ctest --preset bench -V`.

The generated Protobuf C++ comes from `../server/api/proto` (override with
`-DAEDST_PROTO_ROOT=...`). Add `-DVCPKG_MANIFEST_FEATURES=asio -DAEDST_ENABLE_ASIO=ON`
to make Boost.Asio available (see `Modules/Core/Network/include/aedst/network/asio_support.hpp`).

Run it (Debug builds default to `http://localhost:8080`, i.e. `apps/server` running locally; Release builds **require** `AEDST_API_BASE_URL` to be an `https://` URL):

```bash
AEDST_API_BASE_URL=http://localhost:8080 ./build/gcc-debug/App/aedst-desktop
```

On Windows, put `%QT_ROOT_DIR%\bin` on `PATH` (or run `windeployqt`) before
launching from the build tree or running `ctest`. Startup phases are logged as
`startup +N ms: ...` in `<AppData>/logs/aedst.log`.

## Qt Creator

File → Open File or Project → `apps/desktop/CMakeLists.txt`. Qt Creator reads
`CMakePresets.json` and offers the presets as kits; set `VCPKG_ROOT` and
`QT_ROOT_DIR` in the environment Creator starts from (vcpkg installs during the
CMake configure step — no separate command). Set the kit's Qt version to the one
in `qt-version.txt` and leave "QML debugging" **off** for Release kits (configure
refuses `QT_QML_DEBUG` in Release). There are no `.pro`/qmake files.

## Tests

- **GoogleTest / gMock** — Domain use cases, repositories, error model, Connect
  envelope/retry/error vectors, Protobuf golden bytes and the method-table drift
  guard (`Modules/**/tests/*_test.cpp`, CTest label `unit`). `ConnectClient` is
  tested end to end against a loopback `FakeHttpServer` (real
  `QNetworkAccessManager`, real generated messages).
- **Qt Test** — QObject ViewModels with `QSignalSpy`
  (`Modules/Features/Auth/tests/login_view_model_test.cpp`, label `qttest`), run
  with `QT_QPA_PLATFORM=offscreen`; `QBENCHMARK` benchmarks (label `bench`, only
  in the `bench` preset).
- Convenience targets: `cmake --build --preset debug --target check-unit` /
  `check-qttest` / `check`.

Adding an RPC on the server: the desktop build regenerates the messages
automatically; add its entry to `Modules/Core/Contract/include/aedst/contract/methods.hpp`
(the lint script `scripts/check-proto-freshness.sh` and `methods_test.cpp` fail
until you do).

## Static analysis

```bash
# formatting (CI runs this with --dry-run --Werror)
find Modules App -name '*.hpp' -o -name '*.cpp' | xargs clang-format -i

# clang-tidy (blocking in CI; baseline in scripts/clang-tidy-baseline.txt)
./scripts/run-clang-tidy.sh build/clang-debug
# QML lint
./scripts/check-qml-lint.sh
# or build with it inline: cmake --preset gcc-debug -DAEDST_ENABLE_CLANG_TIDY=ON
```

**PVS-Studio** needs a license (free for open source / students, trial
otherwise), so it isn't wired into CI. Local usage against the same
compilation database:

```bash
pvs-studio-analyzer credentials <name> <key>                     # once
pvs-studio-analyzer analyze -f build/gcc-debug/compile_commands.json \
    -e build/ -j8 -o build/pvs.log
plog-converter -a GA:1,2 -t fullhtml build/pvs.log -o build/pvs-report
```

(Windows: `CompilerCommandsAnalyzer.exe analyze --cfg ...` with the same
`compile_commands.json`, or the Visual Studio plugin.)

## Documentation

- [ARCHITECTURE.md](ARCHITECTURE.md) · [CODING_STANDARDS.md](CODING_STANDARDS.md) · [BUILDING.md](BUILDING.md) · [TESTING.md](TESTING.md) · [SECURITY.md](SECURITY.md) · [CONTRIBUTING.md](CONTRIBUTING.md) · [PLATFORM_SUPPORT.md](PLATFORM_SUPPORT.md)
- [ADR-0001 API contract client](docs/adr/0001-api-contract-client.md) - Connect over Qt Network + generated Protobuf, why not gRPC C++
- [ADR-0002 dependency management](docs/adr/0002-dependency-management.md) - vcpkg, Qt pin, CI migration
- [ADR-0003 secret storage](docs/adr/0003-secret-storage.md), [0004 transport policy](docs/adr/0004-transport-policy.md), [0005 database & threading](docs/adr/0005-local-database-and-threading.md), [0006 platform services & updates](docs/adr/0006-platform-services-and-updates.md)
- [Performance baseline](docs/performance.md) - build/runtime switches, measuring, what was deliberately not done

```bash
doxygen Doxyfile        # -> docs/generated/html/index.html (gitignored)
```

## Packaging

From a configured **release** tree (`<release-preset>` = `msvc-release`, `gcc-release`, ...):

| Target / command | Output | Notes |
|---|---|---|
| `cpack --config build/<release-preset>/CPackConfig.cmake` | NSIS `.exe` + `.zip` (Windows), `.deb` + `.tar.gz` (Linux) | CI path |
| `cmake --build --preset <release-preset> --target package_nsis` | NSIS from `packaging/windows/installer.nsi.in` | needs `makensis` |
| `cmake --build --preset <release-preset> --target package_msix` | unsigned `.msix` | needs Windows SDK `makeappx`/`makepri`; signing is a scope cut |
| `cmake --build --preset <release-preset> --target package_appimage` | `.AppImage` | configure with `-DAEDST_DEPLOY_QT=OFF`; needs `linuxdeploy` + Qt plugin and a real icon |
| `cmake --build --preset <release-preset> --target package_flatpak` | `.flatpak` bundle | needs `flatpak-builder`; builds against `org.kde.Platform` 6.12 |

Artifacts land in `build/<preset>/packages/`. Icons are placeholders — see
`App/resources/icons/README.md`.
