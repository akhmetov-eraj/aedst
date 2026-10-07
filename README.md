# AEDST

An empty, AI-friendly project template: a Go backend and four clients in one monorepo, built around one principle — **monolith first, boundaries first, microservices later**. It ships only global modules (auth, tasks as the worked example, search, notifications, media, admin). Product domains are added by each product, never by the template.

| App | Stack | Verified |
|---|---|---|
| [`apps/server`](apps/server/README.md) | Go, Chi, ConnectRPC, Postgres (pgx/sqlc/Goose), River, Valkey, OpenTelemetry | built, vetted, unit-tested |
| [`apps/web`](apps/web/README.md) | Next.js 16, React 19, TanStack Query, shadcn/ui, next-intl | typechecked, linted, unit-tested |
| [`apps/android`](apps/android/README.md) | Kotlin, Jetpack Compose, Hilt, connect-kotlin | **not compiled** |
| [`apps/apple`](apps/apple/README.md) | SwiftUI, GRDB, connect-swift, Tuist (iOS 15+, macOS 12+) | **not built** |
| [`apps/desktop`](apps/desktop/README.md) | C++23, Qt 6, vcpkg, protoc-generated Connect client | **not built** |

Server and Web are the working core. Android, Apple and Desktop were written and reviewed without their toolchains, and their first real build is their acceptance test. See [Platform status](docs/content/status.md) before building on them, including the minimal profile for projects that only need Server + Web.

## For AI agents and contributors

Read in this order before changing code:

1. [`AGENTS.md`](AGENTS.md): the rules in one page.
2. [`CONVENTIONS.md`](CONVENTIONS.md): naming on all five platforms (`scripts/check-naming.sh` enforces it).
3. [`docs/content/library-first.md`](docs/content/library-first.md): the approved stack and the order of preference (standard library → platform API → existing dependency → approved library → custom code).
4. [`docs/content/decisions.md`](docs/content/decisions.md): decisions with rationale and "revisit when". Do not re-propose what is recorded there.
5. The `AGENTS.md` of the app you touch.

## Getting started

Prerequisites: [mise](https://mise.jdx.dev) (pins Go, Node, pnpm, buf, sqlc, Task, golangci-lint, JDK and others in [`mise.toml`](mise.toml)) and Docker (local Postgres and Valkey). Qt, vcpkg, Xcode and the Android SDK are installed separately, only if you work on those apps.

```bash
mise install        # pinned toolchains
task setup          # git hooks, web + docs dependencies, code generation
task dev            # Postgres + Valkey + server (in another shell: task web:dev)
```

`task` is the single entry point; run `task --list` for everything. The common ones:

| Command | What it does |
|---|---|
| `task generate` | buf/sqlc outputs (generated code is never committed) |
| `task lint` / `task test` / `task build` | every platform whose toolchain is installed |
| `task check` | naming, feature boundaries, knip, migration lint, design tokens |
| `task deps:check` / `task deps:update` | outdated dependencies, latest stable only |

Commands per app (for example `task server:test`, `task web:test`) are listed in `task --list`.

## Layout

```
apps/        server, web, android, apple, desktop
packages/    design-tokens (Style Dictionary; generated token files are never hand-edited)
docs/        documentation site (Next.js + Nextra), content in docs/content
scripts/     check-naming.sh, update-deps.sh
```

Architecture: [backend](docs/content/architecture.md), [modules](docs/content/modules.md), [web](docs/content/architecture-web.md), [Android](docs/content/architecture-android.md), [Apple](docs/content/architecture-apple.md), [Desktop](docs/content/architecture-desktop.md). Run the documentation site with `pnpm --dir docs dev`.
