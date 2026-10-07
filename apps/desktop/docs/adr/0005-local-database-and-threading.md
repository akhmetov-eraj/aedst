# ADR-0005: Local database — `user_version` migrations, secure delete, GUI-thread budget

- Status: accepted (not compiled in the authoring environment); the threading decision is superseded by [ADR-0028](0028-local-data-database-thread.md)
- Scope: `apps/desktop` (`Aedst::Persistence`)

## Decision

- **Schema versioning:** `Migrator` applies an ordered list
  (`appMigrations()` in Persistence) inside one transaction per step and bumps
  `PRAGMA user_version` in the same transaction. A database whose version is
  newer than the application knows is **refused** (no silent downgrade).
  Migration #1 is `CREATE TABLE IF NOT EXISTS auth_session`, so pre-migrator
  databases are adopted. `user_version` is one per file, so all features append to
  the single central list.
- **Deletion:** `PRAGMA secure_delete=ON`; `TokenStore::clear()` runs `DELETE`
  and `wal_checkpoint(TRUNCATE)`.
- **Connection settings:** WAL, `synchronous=NORMAL`, `foreign_keys=ON`,
  `busy_timeout=5000`, `SQLITE_OPEN_FULLMUTEX`.
- **Threading contract:** the connection is safe from any thread (FULLMUTEX), but
  today `TokenStore`/`SecureStorage` are called on the GUI thread. That is
  justified by measurement, not assumption: the `PerformanceBudget` test asserts
  100 save+load round trips stay under 50 ms (best of 3). If a budget failure or
  a profile shows GUI stalls (keyring IPC on Linux is the likely culprit), move
  the calls behind an async facade; do not raise the budget.
- **Caching / offline rules:** only the auth session is stored locally today.
  Adding cached domain data requires an entry here: size cap, eviction rule,
  wipe-on-logout, and conflict policy for offline writes.

## Consequences

Sanitizer builds may exceed the budget; if that happens, gate the assertion
on the sanitizer presets in the test itself and record the change here.

## Revisit when

A feature stores user content offline (needs sync/conflict design) or the keyring
call on Linux measurably blocks the UI.
