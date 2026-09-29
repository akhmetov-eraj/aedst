# Database Ownership

Phase 1 runs a single PostgreSQL 18 cluster/database, but **schema-per-module**
from day one — this is what makes `database-per-module` (Phase 2) and
`database-per-service` (Phase 3) a `pg_dump`/restore exercise instead of a
migration project.

| Schema | Owning module | Notes |
|---|---|---|
| `auth` | `auth` | credentials, sessions, refresh tokens, revocation, core identity record — see [ADR-0007](/decisions/0007-merge-users-into-auth) |
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
