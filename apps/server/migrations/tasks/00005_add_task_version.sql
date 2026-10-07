-- +goose Up

-- Optimistic concurrency: every mutation bumps version, and UpdateTask can
-- require the version the client last saw. Adding a column with a constant
-- default is a metadata-only change on PostgreSQL 11+ (no table rewrite).
ALTER TABLE tasks.tasks ADD COLUMN version INTEGER NOT NULL DEFAULT 1;

-- +goose Down
ALTER TABLE tasks.tasks DROP COLUMN version;
