-- +goose no transaction
-- +goose Up

-- CREATE/DROP INDEX CONCURRENTLY cannot run inside a transaction, hence the
-- `no transaction` directive above: on a populated table this avoids the
-- write-blocking lock a plain CREATE INDEX takes.

-- Keyset pagination (see internal/shared/pagination) orders by
-- (sort key, id) in one direction, so the index columns must all share that
-- direction. The old (user_id, created_at DESC, id) index had a mixed
-- direction that ORDER BY created_at DESC, id DESC cannot use; a plain
-- ascending index is scanned in either direction.
CREATE INDEX CONCURRENTLY IF NOT EXISTS idx_tasks_user_created_keyset
    ON tasks.tasks (user_id, created_at, id);

-- Backs ListTasks ordered by due date. Tasks without a due date sort as the
-- earliest instant (see sql/tasks/queries.sql: sentinel) so the ordering is
-- total, keyset-comparable and identical to the previous NULLS FIRST (asc) /
-- NULLS LAST (desc) behaviour. to_timestamp(double precision) is IMMUTABLE,
-- which an expression index requires.
CREATE INDEX CONCURRENTLY IF NOT EXISTS idx_tasks_user_due_keyset
    ON tasks.tasks (user_id, (COALESCE(due_date, to_timestamp(-62135596800))), id);

DROP INDEX CONCURRENTLY IF EXISTS tasks.idx_tasks_user_id_created_at;

-- Substring search (ILIKE '%term%') cannot use a b-tree; trigram GIN indexes
-- make it index-assisted. The extension lives in `public`, shared by every
-- module that needs fuzzy search.
CREATE EXTENSION IF NOT EXISTS pg_trgm;

CREATE INDEX CONCURRENTLY IF NOT EXISTS idx_tasks_title_trgm
    ON tasks.tasks USING gin (title gin_trgm_ops);
CREATE INDEX CONCURRENTLY IF NOT EXISTS idx_tasks_description_trgm
    ON tasks.tasks USING gin (description gin_trgm_ops);

-- +goose Down
DROP INDEX CONCURRENTLY IF EXISTS tasks.idx_tasks_description_trgm;
DROP INDEX CONCURRENTLY IF EXISTS tasks.idx_tasks_title_trgm;
CREATE INDEX CONCURRENTLY IF NOT EXISTS idx_tasks_user_id_created_at
    ON tasks.tasks (user_id, created_at DESC, id);
DROP INDEX CONCURRENTLY IF EXISTS tasks.idx_tasks_user_due_keyset;
DROP INDEX CONCURRENTLY IF EXISTS tasks.idx_tasks_user_created_keyset;
