# aedst-apple

The AEDST Apple client: one Swift codebase, two platform app targets
(`App_iOS`, `App_macOS`), mirroring the module boundaries of `apps/server`
(see [../../docs/content/architecture-apple.md](../../docs/content/architecture-apple.md),
[../../docs/content/architecture.md](../../docs/content/architecture.md) and
[../../docs/content/modules.md](../../docs/content/modules.md) — or the published docs site).

> **This code was written on a Windows machine with no Xcode/Swift toolchain.**
> Nothing in this directory was built, linted, tested or run (CI on `macos-15` is the first thing that will do so) — every
> `Package.swift`, Tuist manifest and Swift source file was written by hand
> against the dependencies' published sources/docs (verified for
> connect-swift 1.2.3, Perception 2.0.x, GRDB 7.x, FlowStacks 0.9.0 and Tuist's
> `ProjectDescription`). Open the project on a Mac and run a build before
> trusting it further — see "Getting started" below, and fix anything the
> compiler flags on first build, especially the `NOTE:` comments in
> `Core/AedstNetwork` and each feature's `Connect*RemoteDataSource.swift`.

## Module map

```
Core/AedstCore            Outcome/AedstError, UseCase, AedstClock, AedstLogger, TokenProvider/SessionStore
Core/AedstNetwork         connect-swift ProtocolClient factory, AuthInterceptor, error mapping, generated/ (gitignored)
Core/AedstPersistence     Keychain-backed SessionStore, GRDB SQLite database (AedstDatabase + DatabaseMigrator migrations)
Core/AedstDesignSystem    Colors/typography/spacing tokens + shared SwiftUI components (AedstButton, AedstTextField, AedstErrorPresenter)
Core/AedstNavigation      AedstRoute protocol + Router (@Perceptible, FlowStacks FlowPath) + AedstNavigationStack — the feature/api boundary
Core/AedstAnalytics       AnalyticsClient / FeatureFlagProvider / AnalyticsConsent facades + no-op defaults
Core/AedstTelemetry       the only SDK importer: Sentry Cocoa (errors, breadcrumbs, URLSession tracing) + PostHog (consent-gated analytics, flags)
Core/AedstMedia           platform-API media kit: cached remote images, AVKit player, thumbnails, ImageIO/AVFoundation compression, PHPicker, background uploader
Core/AedstTesting         ManualClock, StubTokenProvider — shared test doubles

Features/Auth             complete worked example: Domain/Data/Presentation/Navigation
Features/Tasks            task list/detail (complete/reopen/delete), media attachments slot on detail
Features/Search           .searchable + debounced SearchService, U+E000/U+E001 snippet highlighting
Features/Notifications    inbox, unread badge, mark read, preferences, APNs PushCoordinator
Features/Media            library, viewer, TaskAttachmentsView, CreateUpload → presigned PUT → CompleteUpload
Features/Admin            dashboard/users/tasks/broadcast/audit log (entry shown only for the admin role)

App/Shared                AedstAppShared package: AppDependencies (composition root), RootView, AedstAppDelegate (APNs, taps, background uploads), AppLink
App/iOS                   App_iOS Xcode target: @main entry point, Info.plist, Assets.xcassets
App/macOS                 App_macOS Xcode target: @main entry point, entitlements, Info.plist, Assets.xcassets

Project.swift              Tuist project manifest — generates AEDST.xcodeproj (gitignored)
Workspace.swift, Tuist.swift  Tuist workspace / configuration manifests
Tuist/Package.swift        Tuist dependency manifest (integrates the local SwiftPM packages)
Config/                    .xcconfig files (Base/Debug/Release × iOS/macOS)
buf.gen.yaml, Makefile      Connect-Swift codegen from apps/server/api/proto (see Makefile's `generate` target)
```

Dependency direction: `Features/*` → `Core/*`, never the reverse; never
`Features/*` → `Features/*` except through `Core/AedstNavigation`'s route
contracts. See
[../../docs/content/architecture-apple.md](../../docs/content/architecture-apple.md) for the
full rule and how it maps to the server's `api/` boundary and to
`apps/android`'s equivalent.

## Getting started (on a Mac)

Requires Xcode 16.3+ (GRDB 7.11 uses swift-tools-version 6.1; swift-protobuf 1.38 needs Xcode 26 —
SwiftPM picks the newest release your toolchain can build), [Tuist](https://docs.tuist.dev)
(e.g. `mise install tuist`), and the [Buf CLI](https://buf.build/docs/installation)
(`brew install bufbuild/buf/buf`) plus `protoc-gen-swift` and
`protoc-gen-connect-swift` on `PATH` if you want real generated Connect
stubs. **Codegen is mandatory**: `Features/Auth` calls the generated `Aedst_Auth_V1_*` types, so run `make generate` before building or testing anything (`make build` / `make test` do it for you).

```bash
cd apps/apple
make generate   # optional first run — writes Core/AedstNetwork/.../Generated/*.swift
tuist install   # resolves Tuist/Package.swift (local packages + connect-swift, GRDB, Perception, FlowStacks, ...)
tuist generate  # writes AEDST.xcodeproj / AEDST.xcworkspace (gitignored, never commit them) and opens Xcode
```

| Command | What it does |
|---|---|
| `make generate` | `buf generate` against `../server/api/proto` → Swift Connect stubs (Connect codegen, not Tuist) |
| `tuist install` / `make install` | resolve Swift package dependencies declared in `Tuist/Package.swift` |
| `tuist generate` / `make project` | generate the Xcode project + workspace from `Project.swift` |
| `make bootstrap` | `generate` + `install` + `project`, in order |
| `tuist build App_iOS` / `App_macOS` (`make tuist-build`) | build an app scheme |
| `tuist test <scheme>` (`make tuist-test`) | run a scheme's tests (the app schemes have none yet) |
| `make build` | `swift build` every local package (not the app targets) |
| `make test` | `swift test` every local package (where the unit tests live) |
| `make format` | `swift format --in-place --recursive` |
| `make lint` | `swiftlint lint` |

Each package under `Core/` and `Features/` is also a standalone SwiftPM
package — `cd Core/AedstCore && swift test` works without Xcode or
Tuist at all, which is what `.github/workflows/apple-ci.yml` actually
runs in CI (`test` job); the separate `app-build` job generates the Tuist workspace and builds both app schemes unsigned. Releasing (signing, notarization, TestFlight) is in [docs/content/apple-release.md](../../docs/content/apple-release.md).

## Why Tuist instead of a committed `.xcodeproj` (or XcodeGen)

A hand-maintained `.xcodeproj` merges badly (it's an ordered plist keyed by
opaque UUIDs) and drifts silently from the package graph. Tuist replaced
XcodeGen because its manifests are Swift (typed, refactorable, shared helper
functions — see `makeAppTarget` in `Project.swift`), it resolves and
integrates Swift packages itself (`Tuist/Package.swift`), and it is the one
tool that also covers build/test/cache commands. `Project.swift` is the single
source of truth for targets/schemes/deployment targets — regenerate with
`tuist generate` after adding a source folder or changing a build setting,
same spirit as `apps/android`'s `settings.gradle.kts` being hand-authored
while the Gradle-generated IDE project model isn't committed.

Tuist integrates the local packages as generated (static-framework) targets
rather than as Xcode-native local packages; the `Tuist/Package.swift` route was
chosen so third-party versions stay declared once, in the packages themselves.
If a package quirk shows up on first generate (resource bundles in
`Features/Auth`, Swift 6 language mode per target, Perception's macro), the
fallback is Xcode-native integration: declare `packages: [.package(path: "App/Shared")]`
in `Project.swift` and `.package(product: "AedstAppShared")` as the target
dependency, and drop `Tuist/Package.swift`.

## Minimum OS versions

Goal: run on as many OS versions as is reasonable. The floor is set by what
SwiftUI, the app's dependencies and Swift itself support — not by the newest
API the code happens to use; anything newer than the floor is
`#available`-gated or replaced (see "Compatibility notes").

| Platform | Floor | Where declared | Reasoning |
|---|---|---|---|
| iOS / iPadOS | **15.0** | `Project.swift`, every `Package.swift` | SwiftUI below 15 lacks APIs the design system already relies on (`foregroundStyle`, `tint`, `.searchable`, `AsyncImage`, `.task`, `Button(role:)`, `alert` with actions, `Color(uiColor:)`); Perception 2.0.12 also declares 15. Every dependency supports much lower (Swift Concurrency back-deploys to 13, GRDB 13, connect-swift 12, Perception <=2.0.11 13, FlowStacks 14) — SwiftUI maturity is the binding constraint. iOS 15 covers iPhone 6s and later. |
| macOS | **12.0** (Monterey) | `Project.swift`, every `Package.swift` | Same SwiftUI reasoning as iOS 15 (`Color(nsColor:)` 12, `.searchable` 12, `AsyncImage` 12). GRDB/connect-swift/Perception reach 10.15; SwiftUI `App`/`WindowGroup` starts at 11 but is too incomplete there. |
| tvOS | 15.0 (packages only) | Core packages | Scaffold: no app target. Follows iOS. |
| watchOS | 8.0 (packages only) | Core packages | Scaffold: no app target. GRDB needs 7, Perception <=2.0.11 needs 6, SwiftUI parity with iOS 15 is 8. |
| visionOS | 1.0 (packages only) | Core packages | Scaffold: no app target. `Perception.Bindable` is unavailable on visionOS, so `AedstNavigation`, `Auth` and `AedstAppShared` list iOS/macOS only. |

Dependency minimums (verified against the tagged `Package.swift` files):

| Dependency | Version used | Min OS declared | Notes |
|---|---|---|---|
| connect-swift | 1.2.3 (latest release) | iOS 12, macOS 10.15, tvOS 13, watchOS 6 | generated async methods are `@available(iOS 13, *)` |
| swift-protobuf | >= 1.31.0 (latest 1.38.1, tools 6.2) | none declared | connect-swift 1.2.3 requires >= 1.31.0 |
| GRDB.swift | >= 7.0.0 (latest 7.11.1, tools 6.1) | iOS 13, macOS 10.15, tvOS 13, watchOS 7 | |
| swift-perception | 2.0.0 ..< 2.0.12 | iOS 13, macOS 10.15, watchOS 6 | 2.0.12 raised its floor to iOS 15 / macOS 12 and tools to 6.4; we stay below it for toolchain reach and floor headroom |
| FlowStacks | 0.9.0 (0.9.x) | iOS 14, macOS 11, tvOS 14, watchOS 7 | `NavigationStack` back-port |
| Swift Concurrency | — | iOS 13, macOS 10.15 (back-deployed) | `Duration` / `Task.sleep(for:)` are **not** back-deployed (iOS 16) — avoided |

### Compatibility notes (what is gated or replaced because of the floor)

- **Observation** → `swift-perception`: `@Perceptible` replaces `@Observable`
  (`Router`, `LoginViewModel`); views read perceptible state inside
  `WithPerceptionTracking { ... }` and bind through `@Perception.Bindable`. On
  iOS 17 / macOS 14+ these are backed by the system Observation framework. Pass
  `Router` explicitly instead of via `@Environment(Router.self)` (iOS 17 API).
  When the floor reaches iOS 17 / macOS 14, swap back to `@Observable` / `@Bindable`.
- **NavigationStack** (iOS 16) → `AedstNavigationStack` over FlowStacks' `FlowStack`/`FlowPath`
  (renders a real `NavigationStack` where available); features use `.aedstDestination(for:)`.
- **Design system**: `Font.system(_:design:weight:)` → `.weight(_:)`; `AedstToast` takes
  `TimeInterval` (not `Duration`); `AedstSheet` uses its own `AedstSheetDetent` and only applies
  detents/background on iOS 16 / 16.4+; popover compact adaptation (16.4), `scrollContentBackground`
  (16) are gated and degrade to system defaults; `AedstChart` (Swift Charts, iOS 16 / macOS 13) is
  `@available`-gated — callers on iOS 15 / macOS 12 need a fallback; `AedstCombobox` /
  `AedstCommandPalette` use an internal `AedstNavigationContainer` (`NavigationStack` / `NavigationView`).
- **Core**: `AedstClock` uses `TimeInterval` instead of `Duration`.

## Persistence: GRDB (replaces SwiftData)

`Core/AedstPersistence` now provides `AedstDatabase`: a GRDB `DatabasePool`
(on-disk, WAL, concurrent readers + serialized writer) or `DatabaseQueue`
(in-memory, tests), migrated through `DatabaseMigrator`. Base migrations live in
`AedstBaseMigrations`; each feature adds its own via a
`registerMigrations(in: &migrator)` function passed to `AedstDatabase.openPool(at:registering:)`
from the composition root. Access is `try await pool.read/write { db in ... }` —
`DatabaseWriter` is `Sendable`, so no extra actor is needed. Tokens stay in the
Keychain (`KeychainTokenStore`), never in SQLite.

Decision (GRDB vs SQLiteData vs SwiftData):

- **SwiftData** needs iOS 17 / macOS 14 — incompatible with the OS-reach goal. There were no `@Model`
  types or SwiftData tests yet, so nothing needed migrating besides the container factory (removed).
- **SQLiteData** (Point-Free, built on GRDB) is at iOS 16 / macOS 13 since 1.9 (1.6 was iOS 13),
  its newest releases need swift-tools 6.4, and it pulls swift-structured-queries + swift-syntax macros
  and CloudKit-sync machinery we do not use.
- **GRDB directly**: iOS 13 / macOS 10.15, plain SQL/records, first-class `DatabaseMigrator`,
  concurrency-safe pool. SQLiteData sits on top of a GRDB `DatabasePool`, so adopting it later
  (if the OS floor rises to iOS 16 and CloudKit sync or `@FetchAll` become requirements) does not
  invalidate this layer.

## Networking: connect-swift 1.2.3 (Plan B: grpc-swift-2)

connect-swift **1.2.3** (released 2026-05-21) is the latest release (checked on GitHub releases; it
matches the requested version). It is the 1.x stable line (API stable since 1.0). The previously
written `AuthInterceptor` targeted a `UnaryFunction`/`StreamFunction` API that does not exist in any
release; it now implements `UnaryInterceptor` (`handleUnaryRequest` / `handleUnaryResponse`), the
client config uses `ProtoCodec()` and `InterceptorFactory`, and `Code.name` replaces the non-existent
`Code.description` in `NetworkErrorMapping`. Generated clients are async and back-deploy to iOS 13.

**Plan B** if connect-swift stalls (or if the floor rises enough to accept the cost):
[`grpc-swift-2`](https://github.com/grpc/grpc-swift-2) + `grpc-swift-protobuf` +
`grpc-swift-nio-transport` (gRPC over HTTP/2 or gRPC-Web is served by the Connect server too) with
`swift-protobuf` codegen (`protoc-gen-grpc-swift-2`). Trade-offs: its API availability is macOS 15 /
iOS 18 / watchOS 11 / tvOS 18 / visionOS 2 with swift-tools 6.1 — adopting it would end the OS-reach
goal above — and it needs a gRPC-capable transport (NIO) rather than `URLSession`. Only `Core/AedstNetwork`
(and each feature's `Connect*RemoteDataSource`) would change; `Features/*` depend on their own
`*RemoteDataSource` protocols, not on generated types.

## Telemetry, push and media configuration

| Build setting / Info.plist key | Where it comes from | Effect when empty |
|---|---|---|
| `AEDST_SENTRY_DSN` | `Config/Secrets.local.xcconfig` (copy the `.example`) or `xcodebuild AEDST_SENTRY_DSN=...` | Sentry off |
| `AEDST_POSTHOG_KEY`, `AEDST_POSTHOG_HOST` | same | PostHog off, feature flags return their defaults |
| `AEDST_ROLES_CLAIM` | `Config/Base.xcconfig` (`roles`; Keycloak: `realm_access.roles`) | defaults to `roles` |
| `AEDST_APNS_SANDBOX` / `APS_ENVIRONMENT` | `Config/Debug.xcconfig` (YES/development), `Config/Release.xcconfig` (NO/production) | — |

- **Errors**: Sentry is operational (no consent), user id only, no screenshots/view hierarchy/replay. Release name
  `<bundle id>@<version>+<build>`; `scripts/upload-dsyms.sh` uploads dSYMs (CI on main, and the release workflow).
- **Product analytics**: PostHog starts opted out; the "Share anonymous usage analytics" toggle opts in. PostHog's
  push/screen swizzling and crash capture are disabled (Sentry owns crashes; the app owns push).
- **Push**: the App ID needs the Push Notifications capability. The server must send the in-app route as a top-level
  `link` key next to `aps` in the APNs payload; taps route through `AppLink`.
- **Uploads** use a background `URLSession` (`com.aedst.apple.media-upload`); bytes go straight to object storage.
