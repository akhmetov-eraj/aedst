# Architecture — AEDST Desktop

Module Architecture + MVVM + Clean Architecture. The cross-platform rationale
and mapping to the server modules is in
[docs/content/architecture-desktop.md](../../docs/content/architecture-desktop.md);
this file states the boundaries and rules for this app.

## Module graph

```
App/  (composition root: the only target linking every feature)
 ├─ Features/Auth      Domain → Data → Presentation → Navigation
 └─ Core/
     Common        Outcome, Error, UseCase, Clock, AedstLogger (redaction, JSON, correlation), secure_memory
     Contract      generated Protobuf messages + methods.hpp (RPC table)
     Network       ConnectClient (Connect over Qt Network), transport policy, TLS/pinning, retry/deadlines
     Persistence   SqliteConnection/Statement/Migrator, SecureStorage + MasterKeyProvider, TokenStore
     Navigation    Route, NavGraph, Router
     Connectivity  QNetworkInformation wrapper (Offline banner)
     Platform      single instance, deep links, autostart, notifications, platform info
     CrashReporting local capture + consent-gated upload
     Update        signed-manifest verification, semver, rollback protection, artifact hash check
     Analytics     vendor-neutral tracker port (no-op)
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
4. Everything is wired manually in `AppDependencies` (no service locator); QML gets
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
[0006](docs/adr/0006-platform-services-and-updates.md) platform/updates.
