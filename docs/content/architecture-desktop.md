# Desktop Architecture

`apps/desktop` mirrors the same "monolith first, boundaries first" philosophy
as the backend (see [architecture.md](/architecture) and
[modules.md](/modules)) and the same translation `apps/android` and
`apps/apple` already did (see [architecture-android.md](/architecture-android)
and [architecture-apple.md](/architecture-apple)) — translated here to a
C++23 / Qt 6 / CMake project. This doc only covers where desktop diverges and
how it maps onto the other clients; read those first.

> **Verification status: statically reviewed, never compiled.** No
> Qt/CMake/vcpkg toolchain was available in the authoring environment. Nothing
> under `apps/desktop` was configured, built, or run — every `CMakeLists.txt`,
> header, source, and QML file was checked by reading only (target graph,
> include roots, Qt 6.8 CMake API, vcpkg port/target names). **The first real
> configure/build is the acceptance test**: follow "Getting started" in
> [`../apps/desktop/README.md`](../../apps/desktop/README.md) and fix whatever
> it flags. The connectrpc conformance suite for the client is a pending
> follow-up.

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
| `Aedst::Contract` | `Modules/Core/Contract` | `methods.hpp` RPC descriptors; generated Protobuf messages (protoc at build time) |
| `Aedst::Network` | `Modules/Core/Network` | `ConnectClient` (Connect protocol on Qt Network), `AuthInterceptor`, `ApiError`, call options/envelopes |
| `Aedst::Persistence` | `Modules/Core/Persistence` | RAII SQLite (`SqliteConnection`/`SqliteStatement`, vcpkg `sqlite3`), `SecureStorage` + `MasterKeyProvider` (DPAPI / Keychain / libsecret / file fallback), `Migrator`, `TokenStore` |
| `Aedst::Navigation` | `Modules/Core/Navigation` | well-known route names, `NavGraph` (route → QML view), `Router` (back stack driving QML's `StackView`) |
| `Aedst::Connectivity` | `Modules/Core/Connectivity` | `QNetworkInformation` wrapper (Offline state for the UI) |
| `Aedst::Platform` | `Modules/Core/Platform` | single instance, deep-link allow-list parser, autostart, notifications, platform info |
| `Aedst::CrashReporting` | `Modules/Core/CrashReporting` | local crash capture, consent-gated upload |
| `Aedst::Update` | `Modules/Core/Update` | Ed25519-signed manifest verification, semver, rollback protection, artifact hash check |
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
  `Outcome`/`Error` in every Domain signature, `Aedst::Network` for generated messages and
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

## Networking: Connect over Qt Network, generated messages

`Aedst::Contract` + `Aedst::Network` are a **thin Connect-protocol client on
Qt Network with build-time protoc-generated C++ messages** — the desktop
counterpart of Connect-ES / connect-swift / connect-kotlin, against the same
`aedst.auth.v1` / `aedst.tasks.v1` services. Decision and alternatives:
[ADR-0001](../../apps/desktop/docs/adr/0001-api-contract-client.md);
dependency management: [ADR-0002](../../apps/desktop/docs/adr/0002-dependency-management.md).

- **Messages** are generated by `protoc` at build time from
  `apps/server/api/proto` with the same `protoc` that matches libprotobuf.
  Generated code is **not committed**. The old hand-mirrored DTOs and
  "no codegen" approach are gone.
- **`ConnectClient`** (`Modules/Core/Network`) speaks the Connect protocol
  (unary + server streaming, deadlines, cancellation, retries) over one
  long-lived `QNetworkAccessManager`, so TLS, HTTP/2, proxies and the OS trust
  store are Qt's.
- **`contract/methods.hpp`** (`Modules/Core/Contract`) lists every RPC. A new RPC
  **requires an entry there**; a descriptor drift test fails otherwise. Retries
  happen only for methods declared `Idempotent`. Client and bidirectional
  streaming are unsupported — needing them is the trigger to revisit gRPC.
- **Protovalidate**: the proto is vendored in `third_party/protovalidate`, pinned
  to `buf.lock` (`scripts/check-protovalidate-pin.sh`,
  `scripts/check-proto-freshness.sh`).
- **Why custom code is allowed here** (Library-First exception, documented in
  the ADR): no Connect C++ client exists, and gRPC C++ duplicates Qt's
  HTTP/TLS/proxy stack and bypasses the OS certificate store.

Boost.Asio is an opt-in dependency (`AEDST_ENABLE_ASIO`, default `OFF`) with a
single placeholder header, `asio_support.hpp` — Qt Network is the only transport
today. Performance rules for the whole app (LTO, AOT-compiled QML via
`qt_add_qml_module`, model/role-based lists, no `QT_QML_DEBUG` in shipping
builds, startup trace, `QBENCHMARK`) live in
[`performance.md`](../../apps/desktop/docs/performance.md).

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

`Aedst::Persistence` wraps the sqlite3 C API (vcpkg `sqlite3`) instead of
Qt Sql so it works in Qt-free code and pins one SQLite version. Session tokens
are encrypted before they reach SQLite: **Windows DPAPI**
(`CryptProtectData`, user-scoped, no key stored by us) under `#ifdef _WIN32`,
and **OpenSSL AES-256-GCM** with a per-install `0600` key file elsewhere (the
Flatpak/AppImage path). The latter is weaker than a desktop keyring —
libsecret/KWallet integration is a scope cut — but has no dependency on a
Secret Service daemon being reachable from inside a sandbox.

## Build: vcpkg manifest mode + CMake Presets, Qt from official binaries

vcpkg (`vcpkg.json`, pinned by `builtin-baseline`) provides protobuf, openssl,
sqlite3, spdlog/fmt, boost (asio, header-only) and gtest in manifest mode —
dependencies install automatically during `cmake --preset`. **Conan is no
longer used.** **Qt is deliberately not a vcpkg dependency**: it comes as
official binaries (Qt installer or aqtinstall) at the version in
`apps/desktop/qt-version.txt` — the single source of truth, enforced by CMake and
read by CI — and is found through `CMAKE_PREFIX_PATH` (`$QT_ROOT_DIR`).

```bash
export VCPKG_ROOT=~/vcpkg
cmake --preset debug && cmake --build --preset debug && ctest --preset debug
```

Flatpak builds resolve the same `find_package()` names from the KDE runtime and
manifest modules (no network inside `flatpak-builder`).

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

See [modules.md](/modules) for the global module set (`auth`, `tasks`,
`search`, `notifications`, `media`, `admin`). Each has a
`Modules/Features/<Name>` library following the same four-layer shape as
`Modules/Features/Auth`, plus its route name in `Aedst::Navigation`.
