# Database Ownership

Phase 1 runs a single PostgreSQL 18 cluster/database, but **schema-per-module**
from day one — this is what makes `database-per-module` (Phase 2) and
`database-per-service` (Phase 3) a `pg_dump`/restore exercise instead of a
migration project.

| Schema | Owning module | Notes |
|---|---|---|
| `auth` | `auth` | credentials, sessions, refresh tokens, revocation |
| `users` | `users` | core identity record, roles |
| `profile` | `profile` | public profile data |

Rules (see [ADR-0003](decisions/0003-postgresql.md) and
[ADR-0006](decisions/0006-module-boundaries.md)):

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
   owning module's domain events (see [events.md](events.md)) — e.g.
   `marketplace` keeps a `UserSummaryProjection` fed by `ProfileUpdated` instead
   of joining into `profile.*`.

Both patterns survive the module being moved to a separate database; a
cross-schema `JOIN` does not.
