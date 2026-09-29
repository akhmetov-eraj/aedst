# `src/shared` — cross-platform module mapping

This directory is web's equivalent of `core/` (Android), `Core/` (Apple), and
`Modules/Core/` (Desktop): non-feature-specific, reusable infrastructure that
feature modules (`src/modules/*`) depend on but never bypass to reach each
other. The `check:feature-boundaries` script enforces that feature modules
only talk to each other through this layer — mirroring
`apps/android/scripts/check-feature-boundaries.sh`,
`apps/apple/scripts/check-feature-boundaries.sh`, and
`apps/desktop/scripts/check-feature-boundaries.sh`.

Unlike the native platforms, this directory is **not** split into separate
build packages/modules (there is no Gradle/SPM/CMake module boundary in a
Next.js app) — it's one TypeScript package with concern-based sibling
directories, which is the closest Next.js-idiomatic equivalent of the native
per-concern module split. The table below maps each subfolder to its native
counterpart so contributors moving between platforms recognize the
equivalence, even where the web directory keeps its already-idiomatic web
name instead of being physically renamed.

| `src/shared/*` | Native equivalent | Status | Notes |
|---|---|---|---|
| `ui/` | `core/ui` (Android), `AedstDesignSystem` (Apple), `DesignSystem` (Desktop) | **Documented only — not renamed** | Already *is* the web design system: shadcn/ui-generated primitives (`button.tsx`, `dialog.tsx`, ...). Kept as `ui/` because that's the established web/shadcn convention and `CONVENTIONS.md` already documents a naming exception for this directory (kebab-case files, `shadcn` CLI-owned). Renaming would fight the shadcn CLI's own regeneration path for no structural benefit. |
| `api/connect/` | `core/network` (Android), `AedstNetwork` (Apple), `Network` (Desktop) | **Documented only — not renamed** | The ConnectRPC transport/client setup and generated protobuf stubs — this is web's network layer. Left as `api/` rather than renamed to `network/` because (a) "api" is the idiomatic name for this in the web/TypeScript ecosystem, (b) it's referenced extensively in prose across this README, `.env.example`, and `docs/content/decisions/0002-connectrpc.md`, so a rename's documentation churn outweighs the benefit of matching native vocabulary exactly. `api/graphql/` is an unused, superseded stub (see its own `README.md`) kept in place per repo policy against deleting files — it predates ConnectRPC and is not part of the current network layer. |
| `date/` | Not a dedicated native module — closest to a `core/common` (Android) / `AedstCore` (Apple) helper | **Moved** (was `lib/date/`) | Locale-aware `date-fns` formatting helpers. Flattened to a sibling of `ui/`/`api/` (mechanical, 2 importers) because it's a self-contained leaf concern, same as how native `core/common` isolates single-purpose helpers. |
| `env/` | Platform config/environment access (Android `BuildConfig`, Apple `Bundle`/xcconfig, Desktop `QSettings`/env) | **Moved** (was `lib/env/`) | `client.ts` (`NEXT_PUBLIC_*`, safe in the browser bundle) / `server.ts` (`server-only`-guarded secrets). Flattened to a sibling directory (mechanical, ~5 importers) — this is purely a Next.js-specific split (client vs. server bundle boundary) that has no exact native analog, so it gets its own concern-named directory rather than living under a generic `lib/`. |
| `lib/auth/` | Split across `core/network`'s `AuthInterceptor` (Android/Apple: token attachment on outgoing requests) and `core/security` (Android: `EncryptedPreferences` — secure credential storage) | **Documented only — not split** | Contains `auth-interceptor.ts` (network-layer concern), `token-storage.ts` (storage/security concern), and `auth-provider.tsx`/`require-auth.tsx` (React application-state concern, no clean native directory analog since Android/Apple wire equivalent state through DI, not a shared UI-state module). These are kept together because they are one cohesive, small unit on this platform; splitting them across new `network/`+`security/` directories to force an exact native match was judged not worth the churn for ~11 current importers — see `AGENTS.md`'s Library-First guidance against restructuring without a clear technical reason. If this folder grows significantly, revisit splitting `auth-interceptor.ts` into a web `network/` directory alongside `api/connect/`. |
| `lib/hooks/`, `lib/utils/`, `lib/query-client.ts` | `core/common` (Android), `AedstCore` (Apple), `Common` (Desktop) | **Documented only — not renamed** | Small, general-purpose cross-cutting utilities (the `cn()` class-name helper, a `useIsMobile` hook, the shared TanStack Query client instance) with no feature-specific logic — this is exactly what native `core/common`/`Common` holds. Left under the existing, idiomatic `lib/` name (standard in the JS/TS ecosystem) rather than renamed to `common/`, since the value of matching native vocabulary exactly did not outweigh the import churn for a directory whose meaning is already unambiguous on this platform. |

## Feature module internal shape

Native `feature/auth` modules use a consistent internal layout: `Data/`,
`Domain/`, `Navigation/`, `Presentation/` (Apple/Desktop), plus `i18n/` and
`tests/` (Desktop). Web's `src/modules/*` currently use `ui/` (≈
`Presentation/`), `lib/` (≈ a thin `Domain`+`Data` combined — Next.js modules
don't need a separate repository/data-source abstraction layer since
ConnectRPC + TanStack Query hooks are called directly from `ui/`), and `i18n/`
(already matches the native `i18n/` directory 1:1). Web does not have a
`Navigation/` folder per module because Next.js's file-system router
(`src/app/**`) *is* the navigation layer, composed at the `app/` level rather
than declared inside each feature module — there's no separate
navigation-graph module to mirror.

This mapping is intentionally left as documentation rather than an enforced
physical restructure: introducing empty `domain/`/`data/`/`navigation/`
subfolders into `src/modules/{auth,tasks,home}` today would add ceremony
without moving any code (there is no separate data-source/repository
abstraction yet, and no per-module navigation graph to extract), which
contradicts the Library-First "don't add abstraction before there's a
documented technical reason" rule in
[`docs/content/library-first.md`](../../../../docs/content/library-first.md).
Revisit if a module's `lib/` grows enough that separating its data-fetching
code from its pure domain logic becomes genuinely useful.
