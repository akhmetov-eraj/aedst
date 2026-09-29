# core:ui-tv — Aedst TV Design System (Jetpack Compose for TV)

Thin, 10-foot/D-pad-focused companion to [`core:ui`](../ui/README.md), used only by `:app-tv`. See [CONVENTIONS.md](../../../../../CONVENTIONS.md) for the naming rules every file here follows, and [docs/content/library-first.md](../../../../../docs/content/library-first.md) for why `androidx.tv:tv-material` (official Jetpack Compose for TV) is the approved library here rather than a custom leanback/focus framework.

## Why this is a separate module, not a fork of `core:ui`

`core:ui`'s 44 `Aedst*` components are built for touch/phone interaction: Material3 primitives, tap targets, scroll gestures. A 10-foot UI driven by a D-pad has genuinely different requirements — focus traversal instead of touch, scroll-into-view on focus instead of fling gestures, and TV-specific Material components (`androidx.tv.material3`) with their own focus/scale animation contract that Material3's phone components don't expose.

This is the same reasoning Apple's `apps/apple` codebase uses `#if os(iOS)` / `#if os(macOS)` branches for platform divergence inside otherwise-shared code — except Android has no equivalent compile-time branch inside a single Compose source set, so the module boundary (`core:ui` vs. `core:ui-tv`) does that job instead: components that must render differently for D-pad focus live here, named `Tv*` instead of `Aedst*` so both can sit on the classpath of the same consumer without a naming clash; components that don't diverge (theme tokens, spacing) stay in `core:ui` and are reused as-is via an `api` dependency.

## What's reused from `core:ui` as-is

Declared as an `api(project(":core:ui"))` dependency (so `:app-tv` gets these transitively without its own direct dependency on `core:ui`):

- `theme/Theme.kt` — `AedstTheme` (wraps the shared Material3 color scheme/typography/shapes; also used to preview the components below).
- `theme/Spacing.kt` — `AedstSpacing` scale, reused directly for TV layout padding/arrangement.
- `theme/Color.kt`, `theme/Radius.kt`, `theme/Elevation.kt`, `theme/ExtendedColors.kt`, `theme/Type.kt`, `theme/Shape.kt` — same semantic tokens, no TV-specific variant needed.

**Known gap:** `androidx.tv.material3.MaterialTheme` (used internally by `TvPrimaryButton`/`TvTopAppBar`) keeps its own default TV color scheme — it is not yet bridged from `AedstTheme`'s Aedst palette the way `core:ui`'s Material3 `MaterialTheme` is. Porting the full Aedst palette into a `androidx.tv.material3.ColorScheme` (mirroring `core/ui/theme/Theme.kt`'s `LightColors`/`DarkColors`) is a follow-up, not done here to keep this module thin.

## Components (`src/main/kotlin/com/aedst/core/ui/tv/`)

| File | What it wraps | Why it diverges from `core:ui` |
|---|---|---|
| `TvPrimaryButton.kt` | `androidx.tv.material3.Button` | TV `Button` has a distinct focus/scale animation contract vs. Material3's `Button`. |
| `TvTopAppBar.kt` | `androidx.tv.material3.Surface` + `Text` (no native TV `TopAppBar` exists) | Header layout tuned for overscan-safe margins and D-pad focus groups instead of a touch app bar. |
| `TvLazyRow.kt`, `TvLazyColumn.kt` | `androidx.tv.foundation.lazy.list.TvLazyRow` / `TvLazyColumn` | D-pad-navigable lists need focus traversal and scroll-into-view on focus; plain `LazyRow`/`LazyColumn` (used by `core:ui`) have no such semantics. |

All four are thin wrappers, analogous to how `core:ui`'s `Aedst*` components wrap Material3 primitives — no business logic, no duplicated layout engine, just TV-appropriate defaults (Aedst spacing tokens, sane arrangement/state defaults) around the official `androidx.tv` primitives.

Only `androidx.tv:tv-material` is declared in `build.gradle.kts`; `androidx.tv:tv-foundation` (which provides `TvLazyRow`/`TvLazyColumn`) comes in transitively through it.

## Verification status

No JDK/Gradle is available in the sandbox this module was built in, so **no real `./gradlew :core:ui-tv:compileDebugKotlin` has run** — same caveat `core/ui/README.md` already states for its own components. Each file went through a manual self-review pass (import correctness, brace/paren balance, cross-file symbol references) but has not been compiled or run on a device/emulator.

**Before merging:** run a real Gradle build (`./gradlew :core:ui-tv:compileDebugKotlin`), then `ktlint`/`detekt` (see root `CONVENTIONS.md` → Внедрение) — this is a necessary follow-up, not optional.
