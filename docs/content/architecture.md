# AEDST Backend Architecture

## Principle

**Monolith first, boundaries first, microservices later.**

The backend (`apps/server`) ships as a single deployable — `aedst-server` — but is
internally partitioned into independent modules with enforced boundaries. A module
is extracted into its own service only when there is a measured, concrete reason
(traffic, independent scaling/deployment, failure isolation, team ownership). Until
then, nothing outside this repo can tell it isn't already distributed: every
cross-module call goes through the same kind of contract (a Go interface today, a
network RPC tomorrow) and every module owns its own data.

## Current phase

```
Phase 1: Modular Monolith   <-- we are here
Phase 2: Module Services
Phase 3: Domain Microservices
Phase 4: Fine-Grained Services
```

See [modules.md](/modules) for the module list and dependency graph,
[database.md](/database) for data ownership, [events.md](/events) for the
integration-event catalog, and [decisions/](/decisions) for the ADRs behind these
choices.

## Request flow (Phase 1)

```
                 Cloudflare (future)
                        |
                     Envoy (future)
                        |
              ConnectRPC (Connect / gRPC)
                        |
               AEDST Modular Monolith
                        |
        +---------------+---------------+
        |               |               |
     Modules        Platform        Contracts
        |               |               |
     auth            config          proto (buf)
     tasks           database        events
     ...             valkey
                      messaging (in-process bus)
                      outbox (relay)
                      observability
                      auth (jwx)
                        |
        +---------------+---------------+
        |               |               |
   PostgreSQL         Valkey
   (PgBouncer)
```

Locally, `Cloudflare -> Envoy` is skipped: the process listens directly on
`:8080` (ConnectRPC, which also speaks plain HTTP/1.1 + JSON for browsers/curl)
and `:9090` (metrics).

## Repository layout

```
apps/server/
├── cmd/server/            entrypoint: wiring only, no business logic
├── internal/
│   ├── platform/          cross-cutting infrastructure, shared by every module
│   │   ├── config/        env-driven configuration (caarlos0/env)
│   │   ├── database/      pgx pool, tx helpers
│   │   ├── valkey/        Valkey client (go-redis): cache + rate limiting only
│   │   ├── messaging/     in-process event bus (Publisher seam for a broker)
│   │   ├── outbox/        transactional outbox writer + batched relay + lag metrics
│   │   ├── inbox/         consumer-side dedup (exactly-once effect)
│   │   ├── idempotency/   Idempotency-Key replay (per-caller scope, advisory-locked)
│   │   ├── retention/     River periodic purge jobs for append-only tables
│   │   ├── jobs/          River client wiring
│   │   ├── observability/ slog, OpenTelemetry (OTLP); Prometheus /metrics optional
│   │   ├── lock/          Postgres advisory locks (no Redlock/RedSync)
│   │   ├── auth/          OIDC resource-server verification (jwx v4 + jwkfetch); first-party auth is compatibility-only (see [decisions](/decisions))
│   │   └── httpserver/    chi router, middleware stack, ConnectRPC mounting
│   ├── modules/           one bounded context per directory, see modules.md
│   └── shared/            genuinely generic primitives only (ids, pagination,
│                          errors, time) — never a dumping ground
├── api/proto/             contract-first protobuf definitions (buf)
├── gen/                   generated code (buf generate, sqlc generate) — never
│                          hand-edited
├── sql/                   sqlc queries, per module
├── migrations/            goose migrations, per module (schema-per-module)
├── deployments/docker/    docker-compose for local dev
├── architecturetest/      static checks that fail CI on boundary violations
└── Makefile
```

## API contract: proto, Buf, Bruno

`api/proto/` is the single source of truth every client (`apps/web`,
`apps/apple`, and this server's own Go stubs) generates from — see each
app's `buf.gen.yaml`. `apps/server/buf.yaml` is the only lint/breaking
config in the repo: `STANDARD` lint rules, `FILE`-level breaking-change
detection. `make lint` (`apps/server`) runs `buf lint` and
`buf format -d --exit-code`; `make breaking` diffs the current branch
against `main`. CI (`.github/workflows/server-ci.yml`) runs the same three
checks on every PR touching `apps/server/**`, plus a check that `gen/`
isn't stale relative to `api/proto/`.

ConnectRPC has no generated try-it-out page the way OpenAPI/Swagger does —
[`apps/server/api/bruno/`](../../apps/server/api/bruno/README.md) fills
that gap: a git-committed [Bruno](https://www.usebruno.com/) collection
covering every public procedure, with auth-token chaining
(`script:post-response` captures `accessToken`/`refreshToken` after
Register/Login) and negative tests for the 401 (bad token) and 403
(cross-user `UpdateProfile`) paths.

## Module boundary rules

1. A module is a bounded context. It owns its domain model, its tables (own
   Postgres schema), and its own read/write path.
2. Other modules — and `cmd/server` wiring aside — may only depend on a module's
   `api/` package (its public Go interface: commands, queries, DTOs). Importing
   `modules/x/domain`, `modules/x/infrastructure`, `modules/x/repository` or
   `modules/x/transport` from outside module `x` is a boundary violation and is
   caught by `architecturetest` (see [ADR-0006](/decisions/0006-module-boundaries)).
3. Cross-module reads that would otherwise require a SQL join go through the
   owning module's `api/` package, or through a local read-model/projection
   built from that module's domain events — never direct SQL against another
   module's schema.
4. A module's internal layering is one-directional:
   `transport -> application -> domain`, `infrastructure -> application/domain
   interfaces`. `domain` never imports `infrastructure`.
5. Interfaces are defined where they are consumed (e.g. `UserRepository` lives in
   `application`, implemented in `infrastructure/postgres`), not created
   speculatively for every type.

## Extraction path

A module already respects rules 1–4 above, so promoting it to `services/<module>`
is mechanical, not a rewrite:

1. Freeze the module's `api/` contract.
2. Confirm it has zero direct DB access into another module's schema (rule 3
   guarantees this if it was never violated).
3. Replace any remaining in-process calls to the module's `api/` package with a
   ConnectRPC client generated from the same proto contract.
4. Move the module's package tree to its own `cmd/`, give it its own database
   (its schema already has clean ownership boundaries under rule 1).
5. Where the in-process call was previously synchronous but doesn't need to be,
   replace it with an integration event. If the event's consumer becomes its own
   service, plug a broker adapter into the outbox relay's `Publisher` seam
   ([ADR-0011](/decisions/0011-outbox-relay-and-in-process-bus)); producers and
   consumers' inboxes do not change.
6. Deploy independently, measure, and only then consider splitting further.

No module in Phase 1 is extracted preemptively — see
[ADR-0001](/decisions/0001-modular-monolith).

## Scaling

Two things are decided ahead of measurement because their resource profile is
known a priori: **realtime** (long-lived connections) and **media** (large
bodies) are separate deployables from the start — see
[ADR-0008](/decisions/0008-realtime-and-media-extraction). Everything else stays
in the monolith until a measured trigger fires. What to measure, the alert
thresholds, connection-pool budgeting, load-test procedure and known limits are
in the [scaling guide](/scaling); caching and partitioning strategy is in
[ADR-0010](/decisions/0010-caching-and-data-partitioning); client compatibility
in [ADR-0009](/decisions/0009-api-compatibility-and-client-versioning).
