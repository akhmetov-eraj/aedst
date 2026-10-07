# AEDST — AI agent instructions

Read these before modifying any code in this repository:

1. **[CONVENTIONS.md](CONVENTIONS.md)** — naming standard for all 5 platforms (Web, Server, Android, Apple, Desktop). File/type/function/constant naming is a **Must**, enforced by linters and `scripts/check-naming.sh`.
2. **[docs/content/library-first.md](docs/content/library-first.md)** — BOLIPLARE Library-First Development Standard: the approved stack per platform, the mandatory order of preference (standard library → platform API → existing BOLIPLARE dependency → approved library → custom code), the version policy, the explicit rejections, and the monorepo tooling rules.
3. **[docs/content/decisions.md](docs/content/decisions.md)** — index of architectural decisions with rationale and "when to revisit". If a proposal contradicts it, do not implement or re-propose it.
4. The `AGENTS.md` inside the app you touch (e.g. `apps/server/AGENTS.md`, `apps/web/AGENTS.md`) and platform ADRs (`apps/server/docs/adr/`, `apps/desktop/docs/adr/`).

## The one rule that matters most

**Do not write code that an existing library or platform already does.** Before creating a utility, hook, component, service, repository, parser, formatter, validator, or any other new abstraction: check the standard library, the platform API, the libraries already in this repo's `package.json`/`go.mod`/`build.gradle.kts`/`Package.swift`/`vcpkg.json`, and the existing code in this repo — in that order. Only write custom code when none of those fit, and only for a documented technical reason (missing functionality, unsupported platform, incompatible API, unacceptable performance, security/compliance requirement, project-specific domain logic) — never "it's simpler to write myself" or "it's only 50 lines."

This applies to every platform in this repo:
- **Web**: React Hook Form (forms), Zod (validation), date-fns (dates), dnd-kit (drag & drop), shadcn/ui via `apps/web/src/shared/ui/` (UI), TanStack Query/Table/Virtual, nuqs (URL state). Biome is the only linter/formatter.
- **Server**: Chi, Protobuf+Buf+ConnectRPC, Protovalidate, pgx+sqlc+Goose, River (deferred jobs only), Postgres outbox + in-process bus (events), Postgres advisory locks, Valkey (cache/rate limit only), jwx (OIDC resource server), OpenTelemetry (OTLP), caarlos0/env.
- **Android**: Jetpack Compose, Hilt, Coroutines+Flow, connect-kotlin over OkHttp, Keystore + Tink, `apps/android/core/ui/`.
- **Apple**: SwiftUI, swift-perception, FlowStacks, GRDB, connect-swift, Tuist, `apps/apple/Core/AedstDesignSystem/`. Min OS iOS/iPadOS 15, macOS 12.
- **Desktop**: Qt 6 APIs (signals/slots, QTimer, Qt Network) over hand-rolled equivalents, vcpkg, protoc-generated messages + `ConnectClient`, `apps/desktop/Modules/Core/DesignSystem/`.

Full stack tables, anti-patterns, and the AI Definition-of-Done checklist are in `docs/content/library-first.md`.

## Mandatory process rules

- **Versions**: latest stable, verified against the registry. Never invent or guess a version; if you cannot verify it, say `UNVERIFIED`.
- **Approved before first use is machine-enforced**: knip allowlist (`apps/web/knip.jsonc`) on Web; `go mod tidy` + golangci-lint `depguard` on Server. **Adding to any allowlist requires updating `docs/content/library-first.md` in the same change.**
- **Monorepo tooling** (root files created alongside this doc): `mise.toml` pins toolchains; root `Taskfile.yml` (with per-app includes) is the single entry point, so use `task ...` instead of ad-hoc commands; design tokens come from `packages/design-tokens` (Style Dictionary) and generated token files must never be hand-edited.
- **Honesty about verification**: Apple and Desktop were statically reviewed only (no Xcode/Qt toolchain in the authoring environment); the first real build is their acceptance test. Do not claim they build.
- **Docs are the source of truth**: when you change an architectural decision, update `library-first.md` and `decisions.md` in the same change.

## Decision records / forbidden list

Rationale and revisit conditions for everything below: [docs/content/decisions.md](docs/content/decisions.md). Do **not** introduce or re-propose:

- **Server**: Redis (use Valkey, cache/rate-limit only, never correctness); RedSync/Redlock or any Valkey lock (use `internal/platform/lock`); NATS/JetStream/Kafka/RabbitMQ (use outbox + Relay + in-process bus + inbox; never publish from request handlers); a second JWT/JOSE library; an ORM; Prometheus-specific instrumentation; `chimw.RealIP` (use `ClientIPFromXFFTrustedProxies`); self-built identity features (MFA, recovery, social login) — Keycloak is the default IdP and new projects set `AEDST_AUTH_FIRST_PARTY_ENABLED=false`; self-hosted Tempo/Loki/Pyroscope by default.
- **Web**: RxJS (use TanStack Query, `AbortController`, async iterators, Connect streaming); ESLint/Prettier (Biome only); test/dev packages outside `devDependencies`; a new third-party origin without adding it to the CSP in `next.config.ts`.
- **Android**: `security-crypto`/`EncryptedSharedPreferences` (use Keystore + Tink); Retrofit; Koin; `tv-foundation` lists; running on JDK other than 21.
- **Apple**: XcodeGen/`project.yml` (use Tuist); SwiftData (use GRDB); native `@Observable` where iOS < 17 is needed (use swift-perception); `Duration`/`Task.sleep(for:)` and `Environment(_: Observable)`; `grpc-swift-2` as primary transport.
- **Desktop**: Conan (use vcpkg); hand-mirrored DTOs (use protoc-generated messages); committing generated code; an RPC without an entry in `contract/methods.hpp`; production QML via plain `qt_add_resources` (use `qt_add_qml_module`); shipping `QT_QML_DEBUG`.
- **Kept by owner decision (do not propose replacing)**: Next.js (not Vite), react-use, React Hook Form, the `cn` npm package, Chi, Goose (Atlas is paid), Qt 6 / C++23, both loggers (spdlog + Qt) and both test frameworks (GoogleTest + Qt Test), Hilt on AGP 9. See "Explicit rejections" in `library-first.md`.
