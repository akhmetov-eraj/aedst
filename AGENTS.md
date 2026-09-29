# AEDST — AI agent instructions

Read both before modifying any code in this repository:

1. **[CONVENTIONS.md](CONVENTIONS.md)** — naming standard for all 5 platforms (Web, Server, Android, Apple, Desktop). File/type/function/constant naming is a **Must**, enforced by linters and `scripts/check-naming.sh`.
2. **[docs/content/library-first.md](docs/content/library-first.md)** — BOLIPLARE Library-First Development Standard: the approved stack per platform, and the mandatory order of preference (standard library → platform API → existing BOLIPLARE dependency → approved library → custom code) before writing any new implementation.

## The one rule that matters most

**Do not write code that an existing library or platform already does.** Before creating a utility, hook, component, service, repository, parser, formatter, validator, or any other new abstraction: check the standard library, the platform API, the libraries already in this repo's `package.json`/`go.mod`/`build.gradle.kts`/`Package.swift`/`CMakeLists.txt`, and the existing code in this repo — in that order. Only write custom code when none of those fit, and only for a documented technical reason (missing functionality, unsupported platform, incompatible API, unacceptable performance, security/compliance requirement, project-specific domain logic) — never "it's simpler to write myself" or "it's only 50 lines."

This applies to every platform in this repo:
- **Web**: React Hook Form (forms), Zod (validation), date-fns (dates), dnd-kit (drag & drop), shadcn/ui via `apps/web/src/shared/ui/` (UI), TanStack Query/Table/Virtual, nuqs (URL state).
- **Server**: Chi, Protobuf+Buf+ConnectRPC, Protovalidate, pgx+sqlc+Goose, River (jobs), NATS JetStream, Redis, jwx, caarlos0/env.
- **Android**: Jetpack Compose, Hilt, Coroutines+Flow, `apps/android/core/ui/`.
- **Apple**: SwiftUI, Observation, Swift Concurrency, `apps/apple/Core/AedstDesignSystem/`.
- **Desktop**: Qt 6 APIs (signals/slots, QTimer, Qt JSON/Network) over hand-rolled equivalents, `apps/desktop/Modules/Core/DesignSystem/`.

Full stack tables, anti-patterns, and the AI Definition-of-Done checklist are in `docs/content/library-first.md`.
