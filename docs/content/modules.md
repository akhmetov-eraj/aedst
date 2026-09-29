# Modules

Full target module set (from the AEDST product spec). Only the modules marked
**implemented** exist as code today; the rest are documented here so their
boundaries, ownership and dependency direction are decided in advance — adding
one later is additive, not a redesign.

| Module | Status | Owns (schema) | Depends on (via `api/`) |
|---|---|---|---|
| `auth` | **implemented** | `auth.*` (credentials, sessions, identity) | — |
| `tasks` | **implemented** | `tasks.*` | — (own the caller's `user_id` from the auth context; no cross-module call) |
| `social` | planned | `social.*` | `auth` |
| `messenger` | planned | `messenger.*` | `auth` |
| `marketplace` | planned | `marketplace.*` | `auth` |
| `payments` | planned | `payments.*` | `auth` |
| `notifications` | planned | `notifications.*` | events only (no sync calls in) |
| `media` | planned | `media.*` | — |
| `search` | planned | (read-only projections) | events only |
| `recommendations` | planned | (read-only projections) | events only (`social`, `marketplace`) |
| `moderation` | planned | `moderation.*` | events + `api/` calls into target module |
| `admin` | planned | — (composes other modules' `api/`) | many, read-mostly |

## Dependency graph (Phase 1)

```
auth

tasks (no dependencies — every RPC scopes to the caller's own user_id,
       taken from the authenticated context, never from another module)

social ─────→ auth
messenger ──→ auth
marketplace → auth
payments ───→ auth

notifications
    ^
    └── events (from any module)

recommendations
    ^
    ├── social events
    ├── marketplace events
    └── interaction events

search
    ^
    └── events (from any module)

admin ──→ (api/ of whatever it composes; read-mostly, no reverse deps)
```

Rules this graph encodes:

- No cycles. `auth` never imports `social`/`messenger`/`marketplace`/`tasks`.
- `notifications`, `search`, `recommendations` are consumers only — they react to
  domain events published by other modules and never receive synchronous
  in-process calls from them. This is what lets them become independent
  services (or even be rebuilt entirely) without touching the modules that feed
  them.
- `admin` is allowed to depend on many modules' `api/` packages because it is a
  composition layer, but nothing depends on `admin`.

Every new module must add a row + graph edges here as part of the PR that
introduces it, and must be checked by `architecturetest` before merge.

## Module internal structure

```
modules/<name>/
    domain/            entities, value objects, domain errors — no imports
                        outside the standard library and other domain packages
    application/       commands, queries, and the interfaces they need
                        (repositories, publishers) — depends on domain only
    infrastructure/     concrete adapters (postgres/, redis/) implementing
                        application's interfaces
    repository/         sqlc-generated query wrappers used by infrastructure
    transport/           ConnectRPC handlers — depends on application only
    events/              domain/integration event types this module publishes
    api/                the module's public boundary: the only package other
                        modules (or admin) may import
```

`api/` re-exports just enough of `application` (commands, queries, DTOs) for
other modules to call in-process today and over ConnectRPC tomorrow — it is the
seam the [extraction path](/architecture#extraction-path) cuts along.
