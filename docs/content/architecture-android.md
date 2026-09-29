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
| `internal/shared`, `internal/platform` | `core/common`, `core/network`, `core/database`, `core/datastore`, `core/security`, `core/analytics` | cross-cutting, no feature imports these back |
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

See [modules.md](/modules) for the full planned module list (`social`,
`messenger`, `marketplace`, ...) — each, when built, gets a `feature/<name>`
module following the same four-layer shape as `feature/auth`.

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
  need to differ for 10-foot/D-pad UI (`TvPrimaryButton`, `TvTopAppBar`,
  `TvLazyRow`/`TvLazyColumn`) — this module boundary does the same job
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
