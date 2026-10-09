---
title: "Desktop Architecture"
description: "apps/desktop mirrors the same \"monolith first, boundaries first\" philosophy as the backend (see architecture.md and modules.md) and the same translation..."
---

# Desktop Architecture

`apps/desktop` mirrors the same "monolith first, boundaries first" philosophy
as the backend (see [architecture.md](/server/architecture) and
[modules.md](/server/modules)) and the same translation `apps/android` and
`apps/apple` already did (see [architecture-android.md](/android/architecture)
and [architecture-apple.md](/apple/architecture)) — translated here to a
C++23 / Qt 6 / CMake project. This doc only covers where desktop diverges and
how it maps onto the other clients; read those first.

> **Verification status: statically reviewed, never compiled.** No
> Qt/CMake/vcpkg toolchain was available in the authoring environment. Nothing
> under `apps/desktop` was configured, built, or run — every `CMakeLists.txt`,
> header, source, and QML file was checked by reading only (target graph,
> include roots, Qt 6.8 CMake API, vcpkg port/target names). **The first real
> configure/build is the acceptance test**: follow "Getting started" in
> [`../apps/desktop/README.md`](../../../apps/desktop/README.md) and fix whatever
> it flags. The connectrpc conformance suite for the client is a pending
> follow-up.

## Module map

| Server module (`apps/server/internal/modules/`) | Desktop CMake target (`apps/desktop/`) | Status |
|---|---|---|
| `auth` | `Aedst::Feature::Auth` (`Modules/Features/Auth`) | complete worked example (Domain/Data/Presentation/Navigation) |
| `tasks` | `Modules/Features/Tasks` | reference module (never compiled, see [status](/status)); the template for new feature modules next to `Modules/Features/Auth` |
| `internal/shared`, `internal/platform` | `Aedst::Core`, `Aedst::Network`, `Aedst::Persistence`, `Aedst::Analytics` | cross-cutting, no feature links these back into another feature |
| a module's `api/` package | `Aedst::Navigation`'s `routes::*` names + `Router` | the only surface another feature or `App/` may depend on |
| — (client-only) | `Aedst::DesignSystem`, `Aedst::Testing` | QML components/tokens; shared test doubles (test targets only) |
| `cmd/server` wiring | `App/` (`aedst-desktop`, `AppDependencies`) | the composition root |

### Core libraries

| Target | Folder | Purpose |
|---|---|---|
| `Aedst::Core` | `Modules/Core/Common` | Qt-free foundation: `Outcome<T>` (`std::expected`), `Error`, `UseCase`, `Clock`, `AedstLogger` (redaction, JSON, correlation ids), `SecretString`/`secureWipe`, `invokeGuarded` |
| `Aedst::Logging` | `Modules/Core/Logging` | Qt logging categories (`lcApp`, `lcNetwork`, ...); a leaf module so no one needs a dependency on Telemetry to log |
| `Aedst::Contract` | `Modules/Core/Contract` | `methods.hpp` RPC descriptors (+ drift test); generated Protobuf messages (protoc at build time) |
| `Aedst::Network` | `Modules/Core/Network` | `ConnectClient` (Connect protocol on Qt Network: unary + server streaming, retries, deadlines, TLS/SPKI pinning, response size limit), `AuthInterceptor`, `ApiError` |
| `Aedst::Oidc` | `Modules/Core/Oidc` | OpenID Connect sign-in (Authorization Code + PKCE + nonce in the system browser), single-flight refresh, QtKeychain refresh-token store (ADR-0025) |
| `Aedst::Persistence` | `Modules/Core/Persistence` | RAII SQLite, `DatabaseWorker` (the one database thread), `SecureStorage` + `MasterKeyProvider` (DPAPI / libsecret / file fallback), `Migrator` (frozen migrations), `TokenStore`, `KvStore` |
| `Aedst::Outbox` | `Modules/Core/Outbox` | Offline write queue: SQLite outbox table, FIFO drainer, `OutboxService`; features contribute handlers through the plugin `ServiceRegistry` (ADR-0034) |
| `Aedst::Navigation` | `Modules/Core/Navigation` | Route names (`routes::*`, also the `Routes` QML singleton), `NavGraph` (route -> view, link prefixes), `Router` (back stack), `DeepLinkGate` (a link that arrives before sign-in waits for it) |
| `Aedst::Plugin` | `Modules/Core/Plugin` | `IFeaturePlugin`, `ServiceRegistry`, `PluginManager`, capability gating, trust policy (+ optional signed run-time loader); ADR-0033 |
| `Aedst::Capabilities` | `Modules/Core/Capabilities` | `GetCapabilities` client, shown-iff-built-AND-server-enabled rules, last-known cache, version advice (ADR-0035) |
| `Aedst::Connectivity` | `Modules/Core/Connectivity` | `QNetworkInformation` wrapper (Offline state for the UI, outbox, notification polling) |
| `Aedst::Platform` | `Modules/Core/Platform` | single instance, deep-link allow-list parser, autostart, notifications (toast / D-Bus / tray), platform info |
| `Aedst::CrashReporting` | `Modules/Core/CrashReporting` | local crash capture; the consent-gated uploader is library code that no production code wires (Sentry is the upload path) |
| `Aedst::Telemetry` | `Modules/Core/Telemetry` | `SentryReporter`, PostHog tracker, `ErrorCenter`, Qt-log -> spdlog bridge, PII scrubber (ADR-0022, ADR-0030) |
| `Aedst::Update` | `Modules/Core/Update` | Ed25519-signed manifest verification, semver, rollback protection, artifact hash check, startup health guard (ADR-0029) |
| `Aedst::Updater` | `Modules/Core/Updater` | self-update orchestration: `UpdateService` (own worker thread), pipeline, installer port; Velopack adapter (ADR-0029) |
| `Aedst::Analytics` | `Modules/Core/Analytics` | vendor-neutral `AnalyticsTracker` port + `NoOpAnalyticsTracker` |
| `Aedst::Media` | `Modules/Core/Media` | upload controller, presigned uploader, image provider and caches, video frame grabber, compression, `MediaRemoteDataSource` |
| `Aedst::QmlSupport` | `Modules/Core/QmlSupport` | `SingletonSlot<T>` for declarative QML singletons (`QML_ELEMENT` + `QML_SINGLETON`) |
| `Aedst::DesignSystem` | `Modules/Core/DesignSystem` | `ThemeController` + `AedstDesignSystem` QML module (generated tokens, `Theme`, ~45 components, translation loader) |
| `Aedst::Testing` | `Modules/Core/Testing` | test doubles (`FakeClock`, `FakeHttpServer` incl. an HTTPS mode, `StubTokenProvider`, behaviour fixtures); test targets only |

### Feature modules

| Target | Folder | Purpose |
|---|---|---|
| `Aedst::Feature::Auth` | `Modules/Features/Auth` | sign-in (password or browser/OIDC), session restore, `LoginViewModel`; the complete worked example |
| `Aedst::Feature::Tasks` | `Modules/Features/Tasks` | tasks list/detail/create with attachments and offline writes; the reference module |
| `Aedst::Feature::Search` | `Modules/Features/Search` | full-text search with suggestions, highlighted snippets |
| `Aedst::Feature::Notifications` | `Modules/Features/Notifications` | inbox, preferences, live stream with polling fallback, desktop notifications |
| `Aedst::Feature::Media` | `Modules/Features/Media` | "My files" library and the full-screen viewer (uses `Aedst::Media`) |
| `Aedst::Feature::Admin` | `Modules/Features/Admin` | operator screen: overview, users, audit log, broadcast (visible only with the admin role) |

(Generated from `apps/desktop/Modules`; `apps/desktop/scripts/check-docs-modules.sh` fails when a module is missing here.)

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
[ADR-0001](../../../apps/desktop/docs/adr/0001-api-contract-client.md);
dependency management: [ADR-0002](../../../apps/desktop/docs/adr/0002-dependency-management.md).

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

Qt Network is the only transport (Boost.Asio was removed, ADR-0024). Performance rules for the whole app (LTO, AOT-compiled QML via
`qt_add_qml_module`, model/role-based lists, no `QT_QML_DEBUG` in shipping
builds, startup trace, `QBENCHMARK`) live in
[`performance.md`](../../../apps/desktop/docs/performance.md).

## Logging: spdlog vs Qt logging

One sink, two front doors (ADR-0024):

- **`AedstLogger` (spdlog)** — the sink, and the front door for Qt-free code
  (Domain/Data, Persistence, Update, GoogleTest). Console + rotating file
  (`<AppData>/logs/aedst.log`, JSON with `AEDST_LOG_FORMAT=json`), secrets redacted.
- **`QLoggingCategory`** (`lcApp`, `lcNetwork`, `lcPlatform`, `lcMedia`, `lcNavigation`,
  `lcTelemetry` in `Aedst::Logging`) — the front door for Qt-aware code. The Qt message
  handler in `Aedst::Telemetry` forwards `aedst.*` categories at every level and Qt
  framework messages (`qt.*`, QML) from Warning up into spdlog. Filter with
  `QT_LOGGING_RULES` or `AEDST_LOG_RULES`. AEDST code never calls bare `qDebug()`.

## Persistence and secrets

`Aedst::Persistence` wraps the sqlite3 C API (vcpkg `sqlite3`) instead of
Qt Sql so it works in Qt-free code and pins one SQLite version. Session tokens
are encrypted before they reach SQLite: **Windows DPAPI**
(`CryptProtectData`, user-scoped, no key stored by us) under `#ifdef _WIN32`,
and **OpenSSL AES-256-GCM** envelopes elsewhere, with the master key in the
Secret Service through libsecret (`libsecret_backend.cpp`, optional system
package). A per-install `0600` key file is only the logged fallback when no
Secret Service is reachable (some Flatpak/AppImage sandboxes); it is weaker than
a keyring. Refresh tokens in OIDC mode go through QtKeychain
(`keychain_refresh_token_store.cpp`).

## Build: vcpkg manifest mode + CMake Presets, Qt from official binaries

vcpkg (`vcpkg.json`, pinned by `builtin-baseline`) provides protobuf, openssl,
sqlite3, spdlog/fmt and gtest in manifest mode —
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
[`scripts/check-feature-boundaries.sh`](../../../apps/desktop/scripts/check-feature-boundaries.sh)
closes the other half — it greps every `Modules/Features/*/CMakeLists.txt`
for an `Aedst::Feature::*` reference to another feature target and fails CI
(`desktop-ci.yml`'s `lint` job) if it finds one, rather than a configure-time
CMake check or a clang-tidy custom rule. Revisit toward the CMake-side check
if the grep-based version ever produces a false positive (e.g. a target name
that legitimately contains another feature's name as a substring).

## Feature plugins

Every feature is a plugin behind `plugin::IFeaturePlugin` (`Modules/Core/Plugin`, Qt-free): an id, a semver version, the plugin API
version, required server capabilities and an optional feature flag, its `qrc:/` routes, and `registerServices(ServiceRegistry&)`.
`App/src/plugin_host.cpp` adds the six built-ins (Auth, Tasks, Search, Notifications, Media, Admin) and `PluginManager` activates the
ones that pass validation and capability gating (`aedst.capabilities.v1` behind an `ICapabilitySource`; an unknown capability set keeps
built-ins active and holds run-time plugins back). Registration is transactional and a plugin cannot replace a host service or shadow
a built-in route. Optional run-time Qt plugins (`QPluginLoader`, build flag `AEDST_ENABLE_RUNTIME_PLUGINS`, off by default) load only
from `<app dir>/plugins` and only files pinned by an Ed25519-signed allowlist; a release build never loads unsigned or unlisted code.
Plugins are native code in-process, not sandboxed. Details, limits and Phase 2: [ADR-0033](/decisions/desktop). Features still depend on
Core only and never on each other (`check-feature-boundaries.sh` also forbids Core -> feature and feature -> loader links).

## Offline writes, client identification, forced upgrade

Desktop counterpart of Android's outbox ([ADR-0013](/decisions/0013-android-offline-and-client-security)) and the client half of
[ADR-0009](/decisions/0009-api-compatibility-and-client-versioning); decision record `apps/desktop/docs/adr/0034-offline-outbox-and-client-gate.md`.
Behaviour is pinned by the shared fixtures in `docs/contract/behavior/` (`offline-outbox`, `session-refresh`, `client-version-gate`),
which the desktop tests load from the repository file (CMake define `AEDST_BEHAVIOR_FIXTURE_DIR`) and replay against production code.

- **Outbox (`Aedst::Outbox`).** SQLite table `outbox` (migration 2, same database thread as the token store). `OutboxDrainer` delivers in
  creation order; the item id is the `Idempotency-Key` of every attempt (`network::CallOptions::idempotencyKey`); a retryable failure stops
  the run and `OutboxService` backs off (2 s doubling to 5 min, `RetryPolicy`); a permanent failure, an unknown type or the 8th failed
  attempt parks the item. Triggers: enqueue, session resume, `ConnectivityMonitor` going online. Sign-out purges the queue. The table is
  not encrypted: payloads hold only what the operation needs, never tokens or account data.
- **Handlers per feature.** The host puts `OutboxHandlers` and the `ConnectClient` into the plugin `ServiceRegistry`; the Tasks plugin
  registers `task.create` and `task.update` (complete/reopen). A failed create/complete with a network error is queued under the same key
  as the direct call and shown with a "Pending sync" badge and a "Waiting to sync" banner (localized in en, ru, tg, uz, ar).
- **Client identification.** Every API request carries `X-Aedst-Client: desktop/<semver>` (`ClientConfig::clientIdentifier`).
- **Forced upgrade.** `failed_precondition` + `CLIENT_UPGRADE_REQUIRED` is parsed into `ApiError::isUpgradeRequired()` /
  `ErrorCode::UpgradeRequired`; `ConnectClient::clientUpgradeRequired()` makes `AppDependencies` lock the router on the localized
  `UpgradeRequiredView`, which drives the existing update flow (`UpdateService`).

## Scope cut: code signing, store publishing

Desktop targets Windows and Linux only (macOS is served by `apps/apple`, ADR-0023). Only
Windows (NSIS, unsigned MSIX) and Linux (Flatpak, AppImage, DEB/TGZ)
packaging exist. MSIX/Authenticode signing needs a real certificate, and
Flatpak/Flathub publishing needs a repo — both are called out in
`cmake/packaging/*.cmake` and `.github/workflows/desktop-ci.yml` rather than
faked.

See [modules.md](/server/modules) for the global module set (`auth`, `tasks`,
`search`, `notifications`, `media`, `admin`). Each has a
`Modules/Features/<Name>` library following the same four-layer shape as
`Modules/Features/Auth`, plus its route name in `Aedst::Navigation`.

## Media variants

Presets (ADR-0015): `thumb_64` tiny lists, `avatar_256` avatars and small square tiles,
`card_400` grids and cards (the only uncropped preset), `feed_800x600` feeds,
`story_1080x1350` stories. The server returns them as signed imgproxy URLs under
`VariantKind` 5-9 (plus a stored `POSTER` for video); the legacy kinds 1-3 are
`reserved` and gone.
`aedst/media/media_sources.hpp` (`selectVariant`, `urlFor`, `imageSourceFor`)
prefers `avatar_256` for grid thumbnails and the poster / `feed_800x600` for video
previews; an image preview and playback use the original. The original is used
for thumbnails only when no variant exists. **Not compiled:** no Qt/C++ toolchain
was available, so this and `Modules/Core/Media/tests/media_test.cpp` are
unverified until the first CMake build. No RPC changed, so `contract/methods.hpp`
is untouched.

## Capability negotiation

Client half of the capabilities service (`aedst.capabilities.v1`, public RPC); decision record
`apps/desktop/docs/adr/0035-capability-negotiation.md`, behaviour pinned by `docs/contract/behavior/capabilities.json` (replayed by the
desktop tests against the real service).

- **Rules.** A feature is shown only if the build contains it AND the server enables it; unknown server features are ignored; built-ins
  default on and flagged features off when nothing was ever fetched; a failed fetch falls back to the last known answer, then to the
  defaults; `required_client_version` hides a feature on older clients.
- **`Aedst::Capabilities`.** `CapabilitiesService` fetches at launch and when connectivity returns, caches the last answer in the SQLite
  `kv` table (migration 3) and implements the plugin module's `ICapabilitySource`; the shell hides sections through `Session.visibleFeatures`.
- **Version window.** `clients.desktop.minimum` above this build locks the router on the upgrade screen (the same path as the server's
  `CLIENT_UPGRADE_REQUIRED` refusal); the recommended version / `update_status` only raise a non-blocking signal (no UI yet).
