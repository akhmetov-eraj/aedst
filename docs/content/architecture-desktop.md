# Desktop Architecture

`apps/desktop` mirrors the same "monolith first, boundaries first" philosophy
as the backend (see [architecture.md](/architecture) and
[modules.md](/modules)) and the same translation `apps/android` and
`apps/apple` already did (see [architecture-android.md](/architecture-android)
and [architecture-apple.md](/architecture-apple)) — translated here to a
C++23 / Qt 6 / CMake project. This doc only covers where desktop diverges and
how it maps onto the other clients; read those first.

> **This environment had no Qt/Conan toolchain installed** (no `cmake`,
> `conan`, or Qt on `PATH`). Nothing under `apps/desktop` was configured,
> built, or run — every `CMakeLists.txt`, header, source, and QML file was
> written by hand to be internally consistent (target graph, include roots,
> Qt 6.8 CMake API, Conan 2 package/target names). Follow "Getting started" in
> [`../apps/desktop/README.md`](../../apps/desktop/README.md) and fix anything the
> first real configure/compile flags.

## Module map

| Server module (`apps/server/internal/modules/`) | Desktop CMake target (`apps/desktop/`) | Status |
|---|---|---|
| `auth` | `Aedst::Feature::Auth` (`Modules/Features/Auth`) | complete worked example (Domain/Data/Presentation/Navigation) |
| `tasks` | — (not built on Desktop yet) | the reference module lives on server + web only — see [ADR-0007](/decisions/0007-merge-users-into-auth). A native `Modules/Features/Tasks` follows `Modules/Features/Auth`'s shape when built. |
| `internal/shared`, `internal/platform` | `Aedst::Core`, `Aedst::Network`, `Aedst::Persistence`, `Aedst::Analytics` | cross-cutting, no feature links these back into another feature |
| a module's `api/` package | `Aedst::Navigation`'s `routes::*` names + `Router` | the only surface another feature or `App/` may depend on |
| — (client-only) | `Aedst::DesignSystem`, `Aedst::Testing` | QML components/tokens; shared test doubles (test targets only) |
| `cmd/server` wiring | `App/` (`aedst-desktop`, `AppDependencies`) | the composition root |

### Core libraries

| Target | Folder | Purpose |
|---|---|---|
| `Aedst::Core` | `Modules/Core/Common` | Qt-free foundation: `Outcome<T>` (`std::expected`), `Error`, `UseCase`, `Clock`, `AedstLogger` |
| `Aedst::Network` | `Modules/Core/Network` | `ApiClient` (Qt Network, Connect-JSON), `AuthInterceptor`, `ApiError`, procedure paths, hand-mirrored DTOs |
| `Aedst::Persistence` | `Modules/Core/Persistence` | RAII SQLite (`SqliteConnection`/`SqliteStatement`), `SecureStorage` (DPAPI / OpenSSL), `TokenStore` |
| `Aedst::Navigation` | `Modules/Core/Navigation` | well-known route names, `NavGraph` (route → QML view), `Router` (back stack driving QML's `StackView`) |
| `Aedst::Analytics` | `Modules/Core/Analytics` | vendor-neutral `AnalyticsTracker` port + `NoOpAnalyticsTracker` |
| `Aedst::DesignSystem` | `Modules/Core/DesignSystem` | `ThemeController` + `AedstDesignSystem` QML module (`Theme`, `AedstButton`, `AedstTextField`) |
| `Aedst::Testing` | `Modules/Core/Testing` | `FakeClock`, `StubTokenProvider`, a gtest `main()` with `QCoreApplication` |

## The boundary rule, translated

Server rule (ADR-0006): only a module's `api/` package may be imported across
module boundaries; `domain`/`application`/`infrastructure`/`transport` never
leak out. Android enforces its translation with the Gradle module graph, Apple
with the SwiftPM package graph. Desktop's is enforced by the CMake target
graph:

- `Aedst::Feature::*` links `Aedst::*` Core targets. Never the reverse.
- A feature's `target_link_libraries` **never** names another
  `Aedst::Feature::*` target — every feature `CMakeLists.txt` says so in a
  comment at the top. Cross-feature navigation goes through
  `Aedst::Navigation`: a feature pushes another feature's route name (e.g. a
  future `Tasks` feature pushing `routes::kLogin` on sign-out) rather than
  including that feature's header. Same idea as Android's
  `AedstNavGraph`/`AedstRoutes` and Apple's `AedstRoute`/`Router`.
- Linkage is `PRIVATE` by default. A Core target is `PUBLIC` on a feature only
  when that feature's own headers expose its types (`Aedst::Core` for
  `Outcome`/`Error` in every Domain signature, `Aedst::Network` for DTOs and
  `TokenProvider`, `Aedst::Navigation` for `Route`); `Aedst::Persistence`,
  `Aedst::Analytics`, `Aedst::DesignSystem` and `Qt6::Quick` stay `PRIVATE`
  (headers forward-declare). Core targets follow the same rule — e.g. sqlite3
  and OpenSSL are `PRIVATE` to `Aedst::Persistence`, spdlog is `PUBLIC` only on
  `Aedst::Core` because `AedstLogger`'s templates forward into it.
- Each feature's include root is its own directory (`#include
  "Domain/Session.hpp"`), and it only reaches a consumer through
  `target_link_libraries`. So a feature that doesn't link another *cannot even
  include* its headers — the compiler fails, not just the linker.
- `App/` (`aedst-desktop`) is the only target that links every feature. It
  calls each `register<Feature>Destinations(NavGraph&)`, builds every
  repository/use case/ViewModel in `AppDependencies` (manual DI, like
  `AppDependencies.swift`), registers the ViewModels as QML singletons, and
  composes the screens in one `StackView` in `App/qml/Main.qml` — exactly like
  `RootView.swift`, `AppNavHost.kt`, and `cmd/server`.
- When a feature needs something another feature owns, `App/` injects it as a
  plain function or Core interface — e.g. a future `Tasks` feature needing
  the signed-in user id (which Auth owns) would take a
  `CurrentUserIdProvider` use case that `App/` backs with the Core
  `TokenStore`, the same shape `feature/auth`'s own use cases already use.
- Inside a feature, layering is one-directional: `Presentation -> Domain`,
  `Data -> Domain` (implements its repository interfaces), never
  `Domain -> Data`. Domain is pure C++ (only `Aedst::Core`), so every use case
  is testable with plain GoogleTest. See `Modules/Features/Auth` for the fully
  worked layout.

## MVVM with QML

ViewModels are `QObject`s: state is `Q_PROPERTY` + `NOTIFY` (the
`StateFlow`/`@Published` equivalent), one-shot effects are plain signals
(`loginSucceeded`, `loginFailed`) so a re-created view never replays a
navigation — the same UiState-vs-Effect split Android uses. Async completions
guard `this` with `QPointer`. QML views are stateless renderers of their
ViewModel singleton (`import Aedst.Feature.Auth`).

## Networking: Connect over Qt Network, no codegen

`Aedst::Network`'s `ApiClient` speaks ConnectRPC's unary JSON protocol directly
(`POST /<package>.<Service>/<Method>`, `Connect-Protocol-Version: 1`) over
`QNetworkAccessManager`, against the same `aedst.auth.v1` / `aedst.tasks.v1`
services `apps/web` (Connect-ES) and `apps/apple` (connect-swift) call.
Unlike those clients there is no buf/protoc step: the
DTOs in `include/aedst/network/dto/` are hand-mirrored from
`apps/server/api/proto` (proto3 JSON mapping) and must be updated with the
protos. That trade keeps the desktop build to CMake + Conan; revisit if proto
churn makes hand-mirroring error-prone (a `protoc` + `protobuf` Conan
dependency with `JsonStringToMessage` would be the replacement).

Boost.Asio is an opt-in Conan/CMake dependency (`with_asio` /
`AEDST_ENABLE_ASIO`) with a single placeholder header,
`AsioSupport.hpp` — Qt Network is the primary and only transport today.

## Logging: spdlog vs Qt logging

Two logging systems, split on purpose:

- **`AedstLogger` (spdlog)** — all AEDST code: use cases, repositories,
  network, persistence. Console + rotating file (`<AppData>/logs/aedst.log`).
  It is Qt-free, so Domain/Data code and GoogleTest binaries use it without a
  `QCoreApplication`, and the file is what support asks users to attach.
- **Qt `qDebug`/`qWarning` + `QLoggingCategory`** — reserved for Qt/QML
  framework messages (binding warnings, plugin loading, `qt.*` categories).
  Not forwarded into spdlog, so enabling a noisy `qt.*` category via
  `QT_LOGGING_RULES` never floods the product log. AEDST code never calls
  `qDebug()`.

## Persistence and secrets

`Aedst::Persistence` wraps the sqlite3 C API (Conan `sqlite3`) instead of
Qt Sql so it works in Qt-free code and pins one SQLite version. Session tokens
are encrypted before they reach SQLite: **Windows DPAPI**
(`CryptProtectData`, user-scoped, no key stored by us) under `#ifdef _WIN32`,
and **OpenSSL AES-256-GCM** with a per-install `0600` key file elsewhere (the
Flatpak/AppImage path). The latter is weaker than a desktop keyring —
libsecret/KWallet integration is a scope cut — but has no dependency on a
Secret Service daemon being reachable from inside a sandbox.

## Build: Conan 2 + CMake Presets, Qt from the Qt installer

Conan (`conanfile.py`) provides openssl, sqlite3, spdlog/fmt, boost (asio,
header-only) and gtest. **Qt is deliberately not a Conan dependency**: the
ConanCenter `qt` recipe builds Qt from source and is heavy and fragile, so Qt
6.8+ comes from the official Qt installer or aqtinstall and is found through
`CMAKE_PREFIX_PATH` (the presets read `$QT_ROOT_DIR`, which
`jurplel/install-qt-action` also sets in CI).

```bash
conan install . --output-folder=build --build=missing -s build_type=Debug
cmake --preset debug && cmake --build --preset debug && ctest --preset debug
```

`conanfile.py`'s `layout()` pins generators to `build/<preset>/generators`, the
path `CMakePresets.json`'s `toolchainFile` expects. Flatpak builds skip Conan
entirely (no network inside `flatpak-builder`) and resolve the same
`find_package()` names from the KDE runtime and manifest modules.

## Boundary enforcement: `check-feature-boundaries.sh`

Same translation Android and Apple already have (see their docs' equivalent
sections). The include-root trick above makes an accidental cross-feature
`#include` fail to compile, and
[`scripts/check-feature-boundaries.sh`](../../apps/desktop/scripts/check-feature-boundaries.sh)
closes the other half — it greps every `Modules/Features/*/CMakeLists.txt`
for an `Aedst::Feature::*` reference to another feature target and fails CI
(`desktop-ci.yml`'s `lint` job) if it finds one, rather than a configure-time
CMake check or a clang-tidy custom rule. Revisit toward the CMake-side check
if the grep-based version ever produces a false positive (e.g. a target name
that legitimately contains another feature's name as a substring).

## Scope cut: macOS packaging, code signing, store publishing

The CMake/QML code is portable to macOS (`MACOSX_BUNDLE` is set), but only
Windows (NSIS, unsigned MSIX) and Linux (Flatpak, AppImage, DEB/TGZ)
packaging exist. MSIX/Authenticode signing needs a real certificate, and
Flatpak/Flathub publishing needs a repo — both are called out in
`cmake/packaging/*.cmake` and `.github/workflows/desktop-ci.yml` rather than
faked.

See [modules.md](/modules) for the full planned module list (`social`,
`messenger`, `marketplace`, ...) — each, when built, gets a
`Modules/Features/<Name>` library following the same four-layer shape as
`Modules/Features/Auth`, plus its route name in `Aedst::Navigation`.
