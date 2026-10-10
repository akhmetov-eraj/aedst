# Architecture — AEDST Desktop

Module Architecture + MVVM + Clean Architecture. The cross-platform rationale
and mapping to the server modules is in
[docs/content/desktop/architecture.md](../../docs/content/desktop/architecture.md);
this file states the boundaries and rules for this app.

## Module graph

```
App/  (composition root: the only target linking every feature)
 ├─ Features/{Admin,Auth,Media,Notifications,Search,Tasks}   Domain → Data → Presentation → Navigation (Tasks is the reference module)
 └─ Core/
     Common        Outcome, Error, UseCase, Clock, AedstLogger (redaction, JSON, correlation), secure_memory
     Logging       Qt logging categories (leaf module)
     Contract      generated Protobuf messages + methods.hpp (RPC table)
     Network       ConnectClient (Connect over Qt Network), transport policy, TLS/pinning, retry/deadlines, response size limit
     Oidc          OpenID Connect sign-in (PKCE + nonce), refresh-token store in the OS keychain
     Persistence   SqliteConnection/Statement/Migrator, SecureStorage + MasterKeyProvider, TokenStore, kv cache
     Outbox        offline write queue: SQLite outbox table, FIFO drainer, OutboxService (handlers come from features via the plugin ServiceRegistry)
     Navigation    Route, NavGraph, Router
     Plugin        IFeaturePlugin, PluginManager, capability gating, trust policy (+ optional signed run-time loader)
     Capabilities  GetCapabilities client, shown-iff-built-AND-server-enabled rules, last-known cache (kv table), ICapabilitySource, version advice
     Connectivity  QNetworkInformation wrapper (Offline banner)
     Platform      single instance, deep links, autostart, notifications, platform info
     Telemetry     Sentry, PostHog, ErrorCenter, Qt-log bridge, PII scrubber
     CrashReporting local capture (+ unwired consent-gated uploader; Sentry uploads)
     Update        signed-manifest verification, semver, rollback protection, artifact hash check
     Updater       self-update orchestration (UpdateService on its own thread, Velopack adapter)
     Analytics     vendor-neutral tracker port (no-op)
     Media         uploads, image provider/caches, video frames, compression
     QmlSupport    SingletonSlot for declarative QML singletons
     DesignSystem  tokens, Theme, components, translation loader
     Testing       fakes for tests (test targets only)
```

## Rules (enforced by the CMake target graph and `scripts/check-feature-boundaries.sh`)

1. Core never depends on a feature. Features depend on Core only and **never on each
   other**; cross-feature navigation uses route names in `Aedst::Navigation`.
2. Domain is Qt-free (`Aedst::Core`); Data implements Domain ports and speaks
   generated Protobuf messages; Presentation adapts to QML; QML is stateless.
3. Network transport, storage and OS APIs are isolated behind interfaces
   (`TokenProvider`, `MasterKeyProvider`, `NotificationService`, `HttpFetcher`, …);
   platform-specific code is chosen at compile time and guarded by a runtime check.
4. Features are plugins (`Aedst::Plugin`): the composition root adds them, `PluginManager` validates and
   capability-gates them, and only the routes of active plugins join the `NavGraph`. Everything is wired manually in `AppDependencies` (no service locator); QML gets
   its ViewModels and Core singletons registered in `main.cpp`.
5. Threading: GUI thread owns QObjects/QML; SQLite is FULLMUTEX; blocking work is
   off-GUI with explicit cancellation (see ADR-0005).

## Startup sequence (`App/src/main.cpp`)

`--version` short-circuit → `QGuiApplication` → logger → crash handler →
single-instance guard → endpoint validation (fail fast) → `AppDependencies`
(SQLite + migrations, master key provider, token store, network, auth) →
connectivity, translations, notifications → QML engine → activation/deep-link
handling → prewarm network + refresh session.

## Decisions

[ADR-0001](docs/adr/0001-api-contract-client.md) contract client ·
[0002](docs/adr/0002-dependency-management.md) vcpkg + Qt pin ·
[0003](docs/adr/0003-secret-storage.md) secrets ·
[0004](docs/adr/0004-transport-policy.md) transport ·
[0005](docs/adr/0005-local-database-and-threading.md) database/threading ·
[0006](docs/adr/0006-platform-services-and-updates.md) platform/updates ·
[0033](docs/adr/0033-feature-plugin-architecture.md) feature plugins ·
[0034](docs/adr/0034-offline-outbox-and-client-gate.md) offline outbox, Idempotency-Key, client identification, forced upgrade ·
[0035](docs/adr/0035-capability-negotiation.md) capability negotiation, [0036](docs/adr/0036-power-and-resume.md) power and resume.
