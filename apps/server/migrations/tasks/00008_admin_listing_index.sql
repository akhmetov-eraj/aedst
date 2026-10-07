-- +goose no transaction
-- +goose Up

-- Backs the operator listing across all owners (AdminService.ListTasks,
-- keyset on (created_at, id) newest first). Every per-user query already has
-- user_id-leading indexes; this one serves only the cross-tenant admin read.
CREATE INDEX CONCURRENTLY IF NOT EXISTS idx_tasks_created_keyset
    ON tasks.tasks (created_at, id);

-- +goose Down
DROP INDEX CONCURRENTLY IF EXISTS tasks.idx_tasks_created_keyset;
