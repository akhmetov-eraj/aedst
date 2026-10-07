-- +goose no transaction
-- +goose Up

-- Backs the relay's periodic purge of delivered rows (see
-- internal/platform/outbox.Relay): without it, deleting by published_at scans
-- the whole table, and without the purge the outbox grows forever.
CREATE INDEX CONCURRENTLY IF NOT EXISTS idx_tasks_outbox_published
    ON tasks.outbox (published_at)
    WHERE published_at IS NOT NULL;

-- +goose Down
DROP INDEX CONCURRENTLY IF EXISTS tasks.idx_tasks_outbox_published;
