# Apple Architecture

`apps/apple` mirrors the same "monolith first, boundaries first" philosophy
as the backend (see [architecture.md](/architecture) and
[modules.md](/modules)) and the same translation `apps/android` already
did for Gradle (see [architecture-android.md](/architecture-android)) —
translated here to a multi-package Swift project. This doc only covers
where Apple diverges and how it maps onto the other two clients; read those
first.

> **This environment has no Xcode/Swift toolchain installed** (Windows, no
> `swift` on `PATH`). Nothing under `apps/apple` was built, linted, or run —
> every `Package.swift`, Swift source file, and `project.yml` was written by
> hand to be internally consistent (module graph, product/target names,
> import graph, connect-swift's documented API surface). Open the generated
> Xcode project (see "Getting started" in [`../apps/apple/README.md`](../../apps/apple/README.md))
> and run a build before trusting it further — fix anything the compiler
> flags on first build, especially the `NOTE:` comments in
> `Core/AedstNetwork` and each feature's `Connect*RemoteDataSource` file.

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

`project.yml` (read by [XcodeGen](https://github.com/yonaskolb/XcodeGen)) is
what turns this into `AEDST.xcodeproj` with both `App_iOS` and `App_macOS`
targets — the project file itself is generated, never hand-edited or
committed (see the root `.gitignore`), the same way Gradle's
`settings.gradle.kts` is authored by hand but the IDE project model that
comes from it isn't. `apple-ci.yml`'s `xcodegen-validate` job runs `xcodegen
generate` on every PR so a broken `project.yml` (unknown target, bad
dependency reference) fails CI even though a real `xcodebuild` can't run yet
(see "Networking" below).

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
  `Domain/AuthRepository.swift` is the protocol, `Data/AuthRepositoryImpl.swift`
  the implementation, `Presentation/LoginViewModel.swift` depends on the
  protocol via a `UseCase`, never on `Data` directly.

## Networking: generated Connect stubs are not committed

`Core/AedstNetwork` depends on [connect-swift](https://github.com/connectrpc/connect-swift)
and consumes generated stubs the same way `apps/web`'s Connect-ES client and
`apps/server`'s Connect-Go handlers do — from the single proto source of
truth at `apps/server/api/proto`. `apps/apple/buf.gen.yaml` plus `make
generate` (see the root `apps/apple/Makefile`) write those stubs into
`Core/AedstNetwork/Sources/AedstNetwork/Generated/`, which is gitignored.

Every feature's `Data` layer isolates the generated-code dependency to one
file (`Features/Auth/Sources/Auth/Data/ConnectAuthRemoteDataSource.swift` is
the worked example) so the rest of the package graph — and `swift build`
for every other package — compiles before codegen has ever run. That file
is currently a template with a `fatalError` body and a `NOTE:` comment
explaining exactly what to fill in once `make generate` has produced real
`Aedst_Auth_V1_*` types; do that first, then wire the analogous piece for a
future `Features/Tasks` (see this doc's module map).

## Scope cut: watchOS/tvOS/visionOS

Every `Package.swift` under `Core/*` and `Features/*` already declares
`watchOS(.v10)`, `tvOS(.v17)`, and `visionOS(.v1)` platforms, so the shared
business-logic graph is ready for it, but `project.yml` only defines
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
