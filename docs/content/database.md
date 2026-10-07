# Database Ownership

Phase 1 runs a single PostgreSQL 18 cluster/database, but **schema-per-module**
from day one — this is what makes `database-per-module` (Phase 2) and
`database-per-service` (Phase 3) a `pg_dump`/restore exercise instead of a
migration project.

| Schema | Owning module | Notes |
|---|---|---|
| `auth` | `auth` | credentials, sessions, refresh tokens, revocation, core identity record — see [ADR-0007](/decisions/0007-merge-users-into-auth) |
| `notifications` | `notifications` | projection of other modules' events (consumer-only); `user_id` has no FK, like `tasks` |
| `tasks` | `tasks` | the reference module — see [modules.md](/modules). `tasks.tasks.user_id` has no FK to `auth.credentials`: no module references another module's schema, not even via FK (see below) |

Rules (see [ADR-0003](/decisions/0003-postgresql) and
[ADR-0006](/decisions/0006-module-boundaries)):

- A module's Postgres role/connection may only be granted privileges on its own
  schema. No module runs a query that references another module's schema,
  directly or via `JOIN`.
- Migrations live under `migrations/<module>/` and are namespaced per module so
  they can be split into their own repo/pipeline at extraction time without
  touching other modules' migration history.
- `sql/<module>/*.sql` + `sqlc.yaml` generate typed query code into
  `gen/<module>/db`; only `infrastructure/postgres` in that module imports it.
- No ORM. Explicit SQL, explicit transactions (`platform/database` provides a
  `WithTx` helper), explicit indexes/constraints/foreign keys — enforced within
  a schema, never across schemas.
- Connections are pooled through pgx internally and (in front of the server
  process, for horizontal scaling) through PgBouncer in transaction mode.

## Cross-module data

When a module needs data owned by another module:

1. Prefer calling the owning module's `api/` package (in-process today, RPC
   after extraction).
2. If that becomes a hot path or the caller needs to query/filter/sort on the
   foreign data, build a local read-model/projection populated by consuming the
   owning module's domain events (see [events.md](/events)) — e.g. a future
   `notifications` module keeping a `TaskSummaryProjection` fed by a
   `tasks`-published event instead of joining into `tasks.*`. (No module
   currently publishes an integration event — see
   [ADR-0007](/decisions/0007-merge-users-into-auth)'s Consequences — this is
   the pattern the next one that needs to follows.)

Both patterns survive the module being moved to a separate database; a
cross-schema `JOIN` does not.

## Connection pools and PgBouncer

The server holds **two pgx pools**:

| Pool | Env | Used by |
|---|---|---|
| `api` | `AEDST_DATABASE_URL`, `AEDST_DATABASE_MAX_CONNS` (20) | request handlers |
| `jobs` | `AEDST_DATABASE_DIRECT_DSN` (falls back to the main URL), `AEDST_DATABASE_JOBS_MAX_CONNS` (10) | River, the outbox relay, event consumers |

They are separate so a burst of background work can only exhaust its own
connections, never the ones requests need, and a request storm cannot starve
event delivery. Budget: `replicas × (MAX_CONNS + JOBS_MAX_CONNS)` must stay
below Postgres `max_connections` (or PgBouncer's server pool). Both pools
export `aedst_db_pool_*` metrics; alert when `acquired == max` for the `api`
pool or when `aedst_db_pool_empty_acquire_total` keeps rising.

In front of Postgres, run PgBouncer in **transaction mode** for the `api` pool
and point `AEDST_DATABASE_DIRECT_DSN` at Postgres itself: River uses
`LISTEN/NOTIFY` and `platform/lock` takes session-level advisory locks, and
neither survives transaction pooling. See [deployment.md](/deployment).

## Querying rules that keep tables scalable

- **Every owner-scoped table leads its indexes with `user_id`** — the tenant and
  future shard key ([ADR-0010](/decisions/0010-caching-and-data-partitioning)).
- **Keyset pagination, never `OFFSET`**, for any collection that can grow:
  `WHERE (sort_key, id) < ($cursor_key, $cursor_id) ORDER BY sort_key DESC, id
  DESC LIMIT n+1`, backed by an index whose columns all share that direction
  (`(user_id, created_at, id)`). See `internal/shared/pagination`. The response
  carries an opaque `next_page_token`; a total count is computed only for the
  first page, because `count(*)` over the matching set is the same O(rows) cost
  keyset pagination exists to avoid.
- **Substring search uses `pg_trgm`** GIN indexes (`ILIKE '%term%'` cannot use a
  b-tree). LIKE metacharacters in user input are escaped so `%` and `_` match
  literally. Ranked/relevance search belongs in the `search` module, not in a
  business table's query.
- **Indexes on populated tables are created `CONCURRENTLY`** in a
  `-- +goose no transaction` migration; `squawk` (`task lint:migrations`)
  enforces it.
- **Messaging tables are platform-owned** ([ADR-0012](/decisions/0012-shared-platform-messaging-tables)):
  `platform.outbox`, `platform.inbox` and `platform.idempotency_keys` live in
  `migrations/platform`, are written in the module's own transaction, and carry
  a `module` / `consumer` / `scope` column. Modules do not create such tables.
  `platform` is migrated **first** (`task migrate`, the Helm Job, `postgrestest`).
- **Every append-only table has a retention policy**: the outbox purges itself
  (`AEDST_OUTBOX_RETENTION_HOURS`); inbox, idempotency keys and dead sessions are
  purged by River periodic jobs (`internal/platform/retention`,
  `AEDST_RETENTION_*`). A new append-only table ships with its `retention.Policy`
  and a supporting index on the timestamp it purges by. Tables that outgrow
  batched deletes (outbox, notifications, later messages) are the first candidates
  for declarative partitioning.
- **Optimistic concurrency** where users edit shared state: a `version` column
  bumped by every mutation, and an optional `expected_version` on the update RPC
  (`ABORTED` when stale). Updates are single statements (`COALESCE`d columns), not
  read-modify-write, so concurrent edits of different fields never overwrite each
  other. Migrations run with `PGOPTIONS="-c lock_timeout=5s"` so DDL fails fast
  rather than queueing behind traffic.
