# apps/server — AI agent instructions

Read the root [AGENTS.md](../../AGENTS.md) and [CONVENTIONS.md](../../CONVENTIONS.md) first —
this file only adds what's specific to the Go server.

## Before writing a spec-worthy change

A new feature, a new event, or a new command that mutates state should have
a spec under `specs/<module>/<feature>.md` (copy `specs/_template.md`)
**before** implementation — see `.claude/skills/write-spec` if present, or
just fill the template by hand. A one-line bug fix doesn't need one.

## Module boundary rules (enforced, not just documented)

- A module (`internal/modules/<name>/`) is layered
  `domain → application (ports.go + service.go) → infrastructure → transport → api`.
  `domain` depends on nothing but stdlib and `internal/shared/**`.
- Another module is only ever reached through its `api/` package. Direct
  imports of another module's `domain`/`application`/`infrastructure`/`transport`
  are a build-breaking violation — enforced by both `.golangci.yml`'s
  `depguard` rules and `architecturetest/boundaries_test.go` (`task arch`).
- `internal/platform/**` never imports `internal/modules/**` — modules depend
  on platform, never the reverse.
- Banned dependencies (enforced by `architecturetest` and depguard): NATS or any
  external broker, RedSync/Redlock or any distributed lock in Valkey, competing
  JWT libraries, ORMs. Only `internal/platform/valkey` imports the go-redis client.

## When a change touches state and another system cares

There is exactly one asynchronous mechanism family: **Postgres transactional
outbox + inbox + River**. No broker. Do not call `messaging.Publish` from a
request handler, and do not add NATS, Kafka or RabbitMQ: write the event row
via `outbox.Writer` in the same `database.WithTx` as the domain change; the
outbox relay delivers it to `messaging.Publisher` (the in-process bus, where
modules `Subscribe`), and a consuming handler that writes to the database wraps
itself in `internal/platform/inbox` for exactly-once effect. Deferred or
scheduled work is a River job (`internal/platform/jobs`). See
`internal/modules/tasks/infrastructure/postgres/postgres.go`'s `Complete`
method for the reference implementation.

The outbox, inbox and idempotency tables are **shared** (`platform.outbox`,
`platform.inbox`, `platform.idempotency_keys`, ADR-0012): never create per-module
copies, tables or repositories for them. A producing module's whole wiring is
`outbox.NewPostgresRepository(jobsPool, "<module>")` in `cmd/server`. A transition
that publishes an event must be **guarded in SQL** (`UPDATE … WHERE status <> 'done'
RETURNING …`; no row → return the current state, write no event), otherwise a
retry mints a second `event_id` that no inbox will dedupe.

## When a change reacts to another module's event

Copy `internal/modules/notifications`: subscribe in `infrastructure/events`,
import the publisher **only through its `api/` package** (the event type and
payload struct live in `modules/<producer>/api/events.go`), do the effect
inside an `inbox.Consumer` transaction (consumer name = the bus subscription name) on the **jobs pool**, store your own
projection in your own schema. Events only gain fields within a version; a
breaking change is a new `…v2` event published beside the old one. The event
must carry what the consumer needs — never call back into the producer.

## When you add a list that can grow

Keyset pagination, never `OFFSET` (`internal/shared/pagination`; reference:
`modules/tasks` `ListTasks*` queries and `modules/notifications` `List`): order
by `(sort_key, id)`, fetch `limit+1`, index `(user_id, sort_key, id)` with all
columns in one direction, no `count(*)` on the paging path. Filters that are
Go string types go to SQL as `text[]` and are cast there (pgx cannot encode
enum arrays). Substring search uses `pg_trgm` indexes; escape LIKE
metacharacters. Indexes on existing tables are created `CONCURRENTLY` in a
`-- +goose no transaction` migration.

## Connection pools

Two pools: `api` (requests) and `jobs` (River, outbox relay, event consumers).
Code that runs in a background worker or an event subscriber must use the jobs
pool; request handlers use the api pool. Keep
`replicas × (MAX_CONNS + JOBS_MAX_CONNS)` under Postgres `max_connections`.
See `docs/content/scaling.md`.

## When you need mutual exclusion

Use PostgreSQL advisory locks via `internal/platform/lock` (`TryXact` inside a
transaction is preferred; `Locker` for session locks). Never a Redis/Valkey
lock. For leader-only periodic work, River's own leader election/unique jobs.

## When you need cache or rate limiting

Valkey through `internal/platform/valkey` (`RateLimiter`). Rate limiting fails
open by design; never put correctness (locks, dedup, queues) in Valkey.

## Authentication

Handlers never verify tokens: `httpserver.AuthInterceptor` does, through
`platformauth.Verifier` (first-party tokens and/or an external OIDC provider,
see `docs/adr/0001-identity-provider.md`). Read the caller with
`platformauth.FromContext`. Do not add a second token format or verify JWTs
anywhere else; new identity features belong in the IdP, not in the `auth`
module.

## Observability

OpenTelemetry only (`internal/platform/observability`): use `otel.Tracer`/
`otel.Meter` and `slog` with the request context (trace_id is added
automatically). Do not add Prometheus-specific instrumentation, per-vendor
SDKs or a second logger.

## Migrations

Every SQL change needs a goose migration that passes `task lint:migrations`
(squawk). Use `-- +goose NO TRANSACTION` for `CREATE INDEX CONCURRENTLY`.
Run `task gen` after touching `sql/` or `api/proto/`.

## When a command should be safe to retry

Wrap it with `internal/platform/idempotency`: extract the key via
`idempotency.KeyFromContext` in the transport handler, pass it down as an
explicit argument (never read context inside `application`/`domain`), and
call `idempotency.Check`/`Save` inside the same transaction as the write —
see `Repository.Create` in the same file for the reference implementation.
The **scope** you pass (`"<module>:<user id>"`) namespaces the key per caller;
`Check` takes an advisory lock on `(scope, key)`, which is what makes two racing
requests with one key execute once. Hash only the client-supplied fields, never
generated ids or timestamps. Keys are purged by retention after
`AEDST_RETENTION_IDEMPOTENCY_HOURS`.

## When a command edits data users can edit concurrently

One statement, not read-modify-write: `COALESCE(sqlc.narg(x), x)` per column plus
`version = version + 1`, and an optional `expected_version` predicate
(`Repository.Update`). No row back → one follow-up read to tell `NotFound` from a
stale version (`Conflict` → `ABORTED`). Validation lives in `domain` (`NewTask`,
`Patch.Normalize`), never only in the transport or protovalidate. Work that must
not be lost when the change commits (a reminder job) is enqueued in the same
transaction (`client.InsertTx`, unique by args), not after it.

## When a table only grows

Ship a `retention.Policy` with it (`internal/platform/retention`; register in
`cmd/server`) and an index on the timestamp it purges by, created `CONCURRENTLY`.
The purge function deletes at most `limit` rows and returns the count.

## Errors, logging and protection

Return domain errors through `httpserver.ToConnectError`; anything unclassified
becomes a generic `CodeInternal` whose real cause `httpserver.ErrorLogInterceptor`
logs and records on the span — do not log-and-return in handlers. Role checks go
in `AEDST_ROLE_POLICY` (`/aedst.tasks.v1.TasksService/DeleteTask=admin|support`,
enforced by `httpserver.RoleInterceptor`), not in handler code. The policy is
validated at startup against the procedures in the protobuf descriptors: an
unknown or public procedure fails the deploy. A new service is mounted ONLY with
`mountService(rpc, xv1connect.NewXServiceHandler, impl, maxBytes)` (`cmd/server/rpc.go`): that
is what guarantees the protovalidate interceptor, the shared chain, the per-service
message cap and the Connect-Protocol-Version requirement — `cmd/server`'s
`TestEveryConnectHandlerIsMountedThroughTheMounter` fails on a direct
`New*ServiceHandler` call or an RPC passed to `router.Mount`. Add its file
descriptor to `knownProcedures` too (startup fails otherwise); after that an
admin-only RPC needs no wiring change, only an entry in the deployment's
environment. `"/pkg.Service/*=role"` covers a whole service (an exact entry wins);
the built-in default `/aedst.admin.v1.AdminService/*=admin` keeps operator RPCs
closed when `AEDST_ROLE_POLICY` is empty. The interceptor chain is unary-only:
do not add a streaming RPC before the chain implements `WrapStreamingHandler`
(a test enforces this). Every authenticated
RPC is already covered by the per-user rate limit
(`AEDST_API_RATE_LIMIT_PER_MINUTE`) and every request by load shedding
(`AEDST_MAX_IN_FLIGHT`); a limiter outage fails open and must stay that way.
Valkey is optional: never add it to a required readiness check or a startup step.

## Before opening a PR

Run `task check` (gen:check, lint incl. `sqlc vet`, arch, vuln, unit tests (CI adds `-race`) — the
same gate CI runs). `task check:full` additionally runs Docker-dependent gates
(migration linting, integration tests) locally.

## Operator (admin) mutations

The admin module is the one sanctioned exception to "transactions never span
modules": it opens the transaction, passes the `pgx.Tx` to the owning module's
`*Tx` api method (`tasks/api.AdminService.DeleteTaskTx`,
`notifications/api.Broadcaster.BroadcastTx`) and writes `admin.audit_log` in the
same transaction. Only admin may call `*Tx` api methods; the owning module still
writes its own rows and outbox events.

## Background queues and heavy work

River queues: `default` (light jobs) and `media` (image/ffmpeg processing).
`AEDST_JOBS_QUEUES` decides which queues a process works (`default=10,media=2`
by default; a dedicated worker deployment runs `media=4`, API replicas
`default=10`; empty = insert-only). Every process registers every worker
(River validates job kinds on insert). Object storage goes only through
`internal/platform/storage`; push providers only through `internal/platform/push`.
