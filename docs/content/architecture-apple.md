# Apple Architecture

`apps/apple` mirrors the same "monolith first, boundaries first" philosophy
as the backend (see [architecture.md](/architecture) and
[modules.md](/modules)) and the same translation `apps/android` already
did for Gradle (see [architecture-android.md](/architecture-android)) —
translated here to a multi-package Swift project. This doc only covers
where Apple diverges and how it maps onto the other two clients; read those
first.

> **Verification status: statically reviewed, never compiled.** This
> environment (Windows) has no Xcode/Swift toolchain. Nothing under
> `apps/apple` was built, linted, or run — every `Package.swift`,
> `Project.swift`, Swift source file and dependency API usage (connect-swift,
> GRDB, Perception, FlowStacks) was checked by reading only. **The first Xcode
> build is the acceptance test**: run `tuist install && tuist generate` (see
> "Getting started" in [`../apps/apple/README.md`](../../apps/apple/README.md)),
> build, and fix whatever the compiler flags, especially the `NOTE:` comments
> in `Core/AedstNetwork` and each feature's `Connect*RemoteDataSource` file.

## One project, two platform targets, one codebase

Per-platform UI, one shared business-logic graph — the split is:

- **Shared** (`Core/*`, `Features/*`, `App/Shared`): Swift Package Manager
  local packages. No `#if os(...)` in here except the handful of spots that
  need a genuinely different system API (`AedstColor`'s `UIColor`/`NSColor`
  bridging, `AedstButton`'s macOS `.buttonStyle(.plain)`). Everything else —
  domain models, use cases, repositories, networking, persistence, and even
  most SwiftUI views — builds once and runs on both.
- **Platform-specific** (`App/iOS`, `App/macOS`): thin Xcode application
  targets. Each is just an `@main App` entry point, an `Info.plist`, and an
  asset catalog — the platform's own chrome (window sizing, macOS
  `Settings` scene / menu commands, iOS launch screen) and nothing else.
  Both link the same `AedstAppShared` package and render the same
  `RootView`.

The project is described by [Tuist](https://tuist.dev) manifests written in
Swift: `Project.swift` (targets), `Tuist.swift` (config) and
`Tuist/Package.swift` (external dependencies: connect-swift 1.2.3, GRDB 7,
swift-perception, FlowStacks, ...). `tuist install` resolves packages,
`tuist generate` produces `AEDST.xcodeproj` with both `App_iOS` and
`App_macOS` targets, and `tuist build` / `tuist test` drive builds. The
generated project is never hand-edited or committed (gitignored), the same way
Gradle's `settings.gradle.kts` is authored by hand but the IDE project model
that comes from it isn't. **XcodeGen (`project.yml`) is no longer used** — see
[decisions](/decisions). Because Tuist manifests are typed Swift, a broken
target or dependency reference fails at `tuist generate`; this is checked in
CI on a macOS runner only once Xcode builds are possible (see "Networking"
below).

## Minimum OS, persistence and observation

- **OS floors: iOS/iPadOS 15, macOS 12.** SwiftUI maturity is the binding
  limit, not dependencies. Do not use `Duration` / `Task.sleep(for:)` (iOS 16)
  or `Environment(_: Observable)` (iOS 17); gate newer APIs with `#available`.
- **Persistence: GRDB 7** (`DatabasePool` + `DatabaseMigrator`) in
  `Core/AedstPersistence`, replacing SwiftData (needs iOS 17). SQLiteData
  (built on GRDB) is a possible later step.
- **Observation: swift-perception** (`@Perceptible`, `WithPerceptionTracking`,
  `@Perception.Bindable`), pinned `< 2.0.12` to keep iOS 13, so view models work
  below iOS 17.
- **Navigation: FlowStacks** back-ports `NavigationStack`; it is wrapped in
  `AedstNavigation` so features never import it directly.
- **Networking: connect-swift 1.2.3** (`UnaryInterceptor`, `InterceptorFactory`,
  `ProtoCodec`) with swift-protobuf >= 1.31.0. Plan B `grpc-swift-2` needs
  macOS 15 / iOS 18 and would end the OS-reach goal, so it is not the fallback.

## Module map

| Server module (`apps/server/internal/modules/`) | Apple package (`apps/apple/`) | Status |
|---|---|---|
| `auth` | `Features/Auth` | complete worked example (Domain/Data/Presentation/Navigation) |
| `tasks` | — (not built on Apple yet) | the reference module lives on server + web only — see [ADR-0007](/decisions/0007-merge-users-into-auth). A native `Features/Tasks` follows `Features/Auth`'s shape when built. |
| `internal/shared`, `internal/platform` | `Core/AedstCore`, `Core/AedstNetwork`, `Core/AedstPersistence`, `Core/AedstAnalytics` | cross-cutting, no feature imports these back into another feature |
| a module's `api/` package | `Core/AedstNavigation`'s `AedstRoute`/`Router` | the only surface another feature or `App/Shared` may depend on |
| — (Apple-only) | `Core/AedstDesignSystem`, `Core/AedstTesting` | shared SwiftUI components/tokens; shared test doubles |

## The boundary rule, translated

Server rule (ADR-0006): only a module's `api/` package may be imported
across module boundaries; `domain`/`application`/`infrastructure`/`transport`
never leak out. Android's translation (see
[architecture-android.md](/architecture-android)) is enforced by the
Gradle module graph. Apple's is enforced by the SwiftPM package graph:

- `Features/*`'s `Package.swift` depends on `Core/*` packages. Never the
  reverse.
- `Features/*` never adds another `Features/*` package as a dependency.
  Cross-feature navigation goes through `Core/AedstNavigation`'s `AedstRoute`
  protocol and `Router` — a feature pushes a route value another feature
  happens to register a view builder for, the same way a server module
  calls another module's stable RPC contract instead of importing its
  handler package, and the same way Android goes through
  `AedstNavGraph`/`AedstRoutes`.
- `App/Shared` (the `AedstAppShared` package) is the only package allowed to
  depend on every feature — it composes their `*Destinations` view
  modifiers onto one `NavigationStack` in `RootView.swift`, exactly like
  `AppNavHost.kt` on Android and `cmd/server` wiring every module's
  transport handlers on the backend.
- Inside a feature, layering is one-directional: `Presentation -> Domain`,
  `Data -> Domain` (implements its repository protocols), never
  `Domain -> Data`. See `Features/Auth` for the fully worked layout —
  `Domain/AuthRepository.swift` is the protocol, `Data/ConnectAuthRepository.swift`
  the implementation, `Presentation/LoginViewModel.swift` depends on the
  protocol via a `UseCase`, never on `Data` directly.

## Networking: generated Connect stubs are not committed

`Core/AedstNetwork` depends on [connect-swift](https://github.com/connectrpc/connect-swift)
and consumes generated stubs the same way `apps/web`'s Connect-ES client and
`apps/server`'s Connect-Go handlers do — from the single proto source of
truth at `apps/server/api/proto`. `apps/apple/buf.gen.yaml` plus `make
generate` (see the root `apps/apple/Makefile`) write those stubs into
`Core/AedstNetwork/Sources/AedstNetwork/Generated/`, which is gitignored.

Generated stubs are a **build input**, like `sqlc` output on the server: nothing
that compiles `Features/Auth` (unit tests, the app targets) can run before
`make generate`. `make build` / `make test` depend on `generate`, and CI
generates once (`.github/actions/apple-codegen`) and shares the result with
every downstream job. The generated-code dependency is still isolated to one
file per feature (`ConnectAuthRemoteDataSource.swift` is the worked example),
so no other file in the graph names a `Aedst_*_V1_*` type.

### Session, refresh and retry

- **One Keychain item** holds the whole session (user id, token pair, absolute
  expiries) — written with an atomic upsert, so a failed write can never leave
  a mismatched pair (`KeychainTokenStore`). Items are
  `WhenUnlockedThisDeviceOnly`: never backed up, never migrated. Write
  failures are thrown to the caller, not swallowed.
- **Refresh is tri-state** (`RefreshResult`): `refreshed`, `rejected` (the
  server said the refresh token is dead, so the session ends and
  `SessionEvent.expired` fires) and `unavailable` (offline/5xx: the session is
  kept). Concurrent 401s share one in-flight refresh that clears itself from
  inside the actor. Refresh is deliberately *not* auto-retried: refresh tokens
  rotate, and replaying a request whose response was lost could trip the
  server's reuse detection.
- **Retry after 401** lives in `AuthorizedRequestExecutor` (an interceptor
  cannot re-issue a request). `AuthInterceptor` only attaches the bearer token.
- **Offline start**: the session is restored from the Keychain with no network
  call; an expired *access* token is fine (refreshes on demand), an expired
  *refresh* token means signed out. `ConnectivityMonitoring` (NWPathMonitor)
  drives an offline banner.
- **Errors**: `AedstError.errorDescription` is a localized human sentence and
  never contains the technical reason (`technicalDescription` is for logs).
- **Time** always comes from `AedstClock`; tests use `ManualClock`.

### App lock, deep links, diagnostics

- Optional biometric/passcode lock (`AppLockPreference`, `LocalAuthenticator`):
  covers the UI on `inactive`/`background`, requires an unlock to return, and
  enabling it first requires a successful authentication (no self lock-out).
  The preference lives in the Keychain, not `UserDefaults`.
- `aedst://login` is the only accepted custom-scheme link (strict parser,
  `DeepLink`). Universal Links need server-side `apple-app-site-association`
  and a paid team; see [apple-release](/apple-release).
- `DiagnosticsSubscriber` (MetricKit) forwards crash/hang counts (numbers only)
  to `AnalyticsClient`; symbolicated reports come from Xcode Organizer.

## Scope cut: watchOS/tvOS/visionOS

Every `Package.swift` under `Core/*` and `Features/*` already declares
watchOS, tvOS and visionOS platforms (floors listed in the Apple README), so the shared
business-logic graph is ready for it, but `Project.swift` only defines
`App_iOS`/`App_macOS` app targets today. Adding `App_watchOS`/`App_tvOS`/
`App_visionOS` is additive — a new thin app target plus its own
`@main App`, same as `App/macOS` was added alongside `App/iOS` — not a
redesign, deliberately, so this is called out rather than built ahead of an
actual watchOS/tvOS/visionOS product requirement.

## Boundary enforcement: `check-feature-boundaries.sh`

Same translation Android already has (see its doc's equivalent section) —
[`scripts/check-feature-boundaries.sh`](../../apps/apple/scripts/check-feature-boundaries.sh)
greps every `Features/*/Package.swift` for a `.package(path: "../../Features/...")`
reference to another feature package and fails CI (`apple-ci.yml`'s `lint`
job) if it finds one, rather than a SwiftLint custom rule.
