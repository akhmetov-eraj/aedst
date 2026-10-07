# Android Architecture

`apps/android` mirrors the same "monolith first, boundaries first" philosophy
as the backend (see [architecture.md](/architecture) and
[modules.md](/modules)), translated to a multi-module Gradle project. This
doc only covers where the two diverge and how they map onto each other —
read the two linked docs first.

## Module map

| Server module (`apps/server/internal/modules/`) | Android module (`apps/android/`) | Status |
|---|---|---|
| `auth` | `feature/auth` | complete worked example (domain/data/presentation/navigation) |
| `tasks` | — (not built on Android yet) | the reference module lives on server + web only — see [ADR-0007](/decisions/0007-merge-users-into-auth). A native `feature/tasks` follows `feature/auth`'s shape when built. |
| `internal/shared`, `internal/platform` | `core/common`, `core/network`, `core/database`, `core/sync`, `core/datastore`, `core/security`, `core/analytics` | cross-cutting, no feature imports these back |
| a module's `api/` package | `core/navigation`'s `AedstNavGraph`/`AedstRoutes` | the only surface another feature or `app/` may depend on |

## The boundary rule, translated

Server rule (ADR-0006): only a module's `api/` package may be imported across
module boundaries; `domain`/`application`/`infrastructure`/`transport` never
leak out.

Android equivalent, enforced by Gradle module graph rather than a static
analyzer (see "Scope cut" below):

- `feature/*` depends on `core/*`. Never the reverse.
- `feature/*` never depends on another `feature/*` module's Gradle target.
  Cross-feature navigation goes through `core/navigation`'s `AedstNavGraph`
  interface and the well-known route constants in `AedstRoutes` — a feature
  navigates to a route string another feature happens to register, the same
  way a server module calls another module's stable RPC contract instead of
  importing its handler package.
- `app/` is the only module allowed to see every feature — it composes their
  `navigation` entry points into one `NavHost` ([AppNavHost.kt](../../apps/android/app/src/main/kotlin/com/aedst/android/navigation/AppNavHost.kt)),
  exactly like `cmd/server` wires every module's transport handlers.
- Inside a feature, layering is one-directional: `presentation -> domain`,
  `data -> domain` (implements its repository interfaces), never
  `domain -> data`. See `feature/auth` for the fully worked layout.

## Boundary enforcement: `check-feature-boundaries.sh`

The server enforces its boundary rules with a static analyzer that fails CI
(ADR-0006). Android's equivalent is
[`scripts/check-feature-boundaries.sh`](../../apps/android/scripts/check-feature-boundaries.sh),
wired into `android-ci.yml`'s `lint` job: it greps every `feature/*/build.gradle.kts`
for a `project(":feature:...")` reference to another feature module and fails
the build if it finds one. It's a grep-based check, not a Gradle plugin or a
`detekt` custom rule, deliberately — it needed no new build-logic dependency
and catches the one mistake this rule actually guards against (a feature
declaring a Gradle dependency on another feature). Revisit if the module
graph outgrows what a per-file grep can express.

See [modules.md](/modules) for the global module set (`auth`, `tasks`,
`search`, `notifications`, `media`, `admin`). Each has a `feature/<name>`
module following the same four-layer shape as `feature/auth`; a product built
on the template adds its own domain modules the same way.

## Phone and TV: a second thin app target

Android now mirrors the pattern [architecture-apple.md](/architecture-apple)
already documents for iOS/macOS: two **thin** platform app modules sharing
100% of business logic and non-diverging UI through the same `core/*` /
`feature/*` module graph, rather than a forked app.

- **`:app`** — phone/tablet, unchanged structurally (`MainActivity`,
  `AedstApplication`, `AppNavHost`).
- **`:app-tv`** (`apps/android/app-tv/`) — Android TV, its own
  `AndroidManifest.xml` (`android.software.leanback` required,
  `LEANBACK_LAUNCHER` instead of `LAUNCHER`), its own minimal
  `MainTvActivity`/`AedstTvApplication` entry point, wiring the exact same
  `AuthNavGraph()` from `:feature:auth` — zero business-logic duplication,
  the same way `App_iOS`/`App_macOS` both link one `AedstAppShared` package.
- **`:core:ui`** stays the shared design system both targets can use for
  anything that doesn't need to diverge (theme tokens, non-focus-sensitive
  pieces).
- **`:core:ui-tv`** (`apps/android/core/ui-tv/`) — a thin module, *not* a
  fork of `:core:ui`, used only by `:app-tv`. Depends on `:core:ui` (`api`,
  for token/theme reuse) and `androidx.tv:tv-material` (official Jetpack
  Compose for TV library — approved via library-first.md's "Platform API"
  tier). Holds only the handful of `Tv*`-prefixed components that genuinely
  need to differ for 10-foot/D-pad UI (`TvPrimaryButton`, `TvTopAppBar`; lists use
  standard Compose `LazyRow`/`LazyColumn`, because `tv-foundation` is deprecated) — this module boundary does the same job
  Apple's `#if os(iOS)`/`#if os(macOS)` branches do inside one shared
  SwiftUI package, just expressed as a Gradle module split instead of a
  compile-time directive, since Kotlin/JVM has no OS-conditional-compilation
  equivalent.

```
:app        -> :core:common, :core:ui, :core:navigation, :core:analytics, :core:security, :feature:auth
:app-tv     -> :core:common, :core:ui-tv, :core:navigation, :core:analytics, :core:security, :feature:auth
:core:ui-tv -> :core:ui (api), androidx.tv:tv-material (api)
```

See [`apps/android/core/ui-tv/README.md`](../../apps/android/core/ui-tv/README.md)
for the component-level detail and its own unverified-build caveat (no
Android SDK/Gradle toolchain in the environment this was written in).

## Toolchain, networking and secrets (current decisions)

Full rationale and revisit conditions: [decisions](/decisions); stack table:
[library-first](/library-first#android).

- **JDK 21 required** (detekt 1.23 fails on JDK 25). Run `make generate` (buf)
  before the first build; generated stubs are not committed. Versions are
  latest-stable only, with the Compose BOM applied through `platform()`.
- **DI: Hilt on AGP 9** with built-in Kotlin (no explicit `kotlin-android`
  plugin). Metro only if KMP appears; Koin was rejected (no compile-time checks).
- **Networking: connect-kotlin 0.9.0 over OkHttp 5.5.0** (compileSdk 37); no
  Retrofit. Plan B if connect-kotlin stalls: Wire / gRPC-Kotlin (the server also
  serves gRPC and gRPC-Web).
- **Secrets: Android Keystore + Tink AEAD** (`AndroidKeysetManager`) with an
  encrypted DataStore serializer. `androidx.security:security-crypto`
  (`EncryptedSharedPreferences`) is deprecated and no longer used; legacy data is
  migrated read, write, delete.

## Session, offline and client protection (current decisions)

Rationale and revisit conditions: [ADR-0013](/decisions/0013-android-offline-and-client-security).

```
request ──► RequestHeadersInterceptor (X-Aedst-Client, traceparent)
        ──► AuthInterceptor (Bearer from AuthTokenStore)
        ──► server ── 401 ──► TokenRefreshAuthenticator ──► TokenRefresher (feature/auth: mutex, RefreshToken RPC)
                                                          ├─ ok        → replay once with the new token
                                                          ├─ rejected  → session cleared → SessionState=false → app/ routes to login
                                                          └─ transient → 401 surfaces, session kept
```

- **Who owns what.** `core/network` declares the contracts (`AuthTokenProvider`, `TokenRefresher`); `feature/auth` implements them in `AuthModule`; `core/common` declares `SessionState`, which `app/` reads. `app/` never imports `feature/auth`'s domain or data packages.
- **Offline writes.** A feature persists an `OutboxEntity` through `OutboxRepository.enqueue(type, payload)` and registers one `OutboxHandler` (`@Binds @IntoSet`). `OutboxSyncWorker` delivers in creation order when the network is connected, with exponential backoff; the row id is the `Idempotency-Key`. Retryable errors (`Network`, `Offline`) keep the item; anything else parks it as `FAILED`.
- **Errors reach users as kinds, not text.** ViewModels put an `AedstError` in state; the composable calls `error.userMessage()` (`core/ui`), which maps the kind to a localized string.
- **Data classes and where they live.** Secrets → `core/security` (Keystore + Tink). Non-sensitive, re-fetchable data and the outbox → Room (unencrypted). Preferences → DataStore. Nothing is backed up (`allowBackup=false`).
- **Release gate.** `:app:preReleaseBuild` depends on `:core:network:verifyReleaseCertPins`; pass `-Paedst.certPins=sha256/<live>,sha256/<backup>` (or, deliberately, `-Paedst.allowUnpinnedRelease=true`).
- **Tests.** JVM (`testDebugUnitTest`): JUnit, MockK, Turbine, MockWebServer, and Robolectric for Compose, Room and Worker tests. Instrumented (`connectedDebugAndroidTest`): Compose UI tests with Accessibility Test Framework checks enabled (`enableAccessibilityChecks()`), run on an emulator in CI.
