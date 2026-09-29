# aedst-apple

The AEDST Apple client: one Swift codebase, two platform app targets
(`App_iOS`, `App_macOS`), mirroring the module boundaries of `apps/server`
(see [../../docs/content/architecture-apple.md](../../docs/content/architecture-apple.md),
[../../docs/content/architecture.md](../../docs/content/architecture.md) and
[../../docs/content/modules.md](../../docs/content/modules.md) — or the published docs site).

> **This environment has no Xcode/Swift toolchain installed** (this was
> written on Windows — `swift`/`xcodegen`/`xcodebuild` are not on `PATH`
> here). Nothing in this directory was built, linted, or run — every
> `Package.swift` and Swift source file was written by hand to be
> internally consistent (module graph, product names, import graph,
> connect-swift's documented API shape). Open the project on a Mac and run
> a build before trusting it further — see "Getting started" below, and
> fix anything the compiler flags on first build, especially the `NOTE:`
> comments in `Core/AedstNetwork` and each feature's
> `Connect*RemoteDataSource.swift`.

## Module map

```
Core/AedstCore            Outcome/AedstError, UseCase, AedstClock, AedstLogger, TokenProvider/SessionStore
Core/AedstNetwork         connect-swift ProtocolClient factory, AuthInterceptor, error mapping, generated/ (gitignored)
Core/AedstPersistence     Keychain-backed SessionStore, SwiftData ModelContainer factory
Core/AedstDesignSystem    Colors/typography/spacing tokens + shared SwiftUI components (AedstButton, AedstTextField)
Core/AedstNavigation      AedstRoute protocol + Router (NavigationPath) — the feature/api boundary
Core/AedstAnalytics       Analytics/crash-reporting facade + no-op default
Core/AedstTesting         ManualClock, StubTokenProvider — shared test doubles

Features/Auth             complete worked example: Domain/Data/Presentation/Navigation

App/Shared                AedstAppShared package: AppDependencies (composition root) + RootView
App/iOS                   App_iOS Xcode target: @main entry point, Info.plist, Assets.xcassets
App/macOS                 App_macOS Xcode target: @main entry point, entitlements, Info.plist, Assets.xcassets

project.yml                XcodeGen spec — generates AEDST.xcodeproj (gitignored)
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

Requires Xcode 16+, [XcodeGen](https://github.com/yonaskolb/XcodeGen)
(`brew install xcodegen`), and the [Buf CLI](https://buf.build/docs/installation)
(`brew install bufbuild/buf/buf`) plus `protoc-gen-swift` and
`protoc-gen-connect-swift` on `PATH` if you want real generated Connect
stubs (`make generate`) rather than the `fatalError`-template data sources.

```bash
cd apps/apple
make generate   # optional first run — writes Core/AedstNetwork/.../Generated/*.swift
make project    # xcodegen generate — writes AEDST.xcodeproj (gitignored, never commit it)
open AEDST.xcodeproj
```

| Command | What it does |
|---|---|
| `make generate` | `buf generate` against `../server/api/proto` → Swift Connect stubs |
| `make project` | `xcodegen generate` → `AEDST.xcodeproj` |
| `make bootstrap` | both of the above, in order |
| `make build` | `swift build` every local package (not the `.xcodeproj` app targets) |
| `make test` | `swift test` every local package |
| `make format` | `swift format --in-place --recursive` |
| `make lint` | `swiftlint lint` |

Each package under `Core/` and `Features/` is also a standalone SwiftPM
package — `cd Core/AedstCore && swift test` works without Xcode or
XcodeGen at all, which is what `.github/workflows/apple-ci.yml` actually
runs in CI (it does not build the `.xcodeproj`; see that workflow's
comments for why).

## Why XcodeGen instead of a committed `.xcodeproj`

A hand-maintained `.xcodeproj` merges badly (it's an ordered plist keyed by
opaque UUIDs) and drifts silently from the package graph. `project.yml` is
the single source of truth for targets/schemes/settings — regenerate with
`make project` after adding a source folder or changing a build setting,
same spirit as `apps/android`'s `settings.gradle.kts` being hand-authored
while the Gradle-generated IDE project model isn't committed.
