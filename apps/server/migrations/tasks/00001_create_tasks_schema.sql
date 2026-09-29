-- +goose Up
CREATE SCHEMA IF NOT EXISTS tasks;

CREATE TYPE tasks.status AS ENUM ('todo', 'in_progress', 'done');
CREATE TYPE tasks.priority AS ENUM ('low', 'medium', 'high', 'urgent');

-- No FK from user_id to auth.credentials: modules never reference another
-- module's schema, not even via FK (see docs/database.md) — a cross-schema
-- FK would be the same coupling a cross-schema JOIN is, just enforced by a
-- constraint instead of a query. Validity of user_id comes from the JWT
-- AuthInterceptor already verified before a handler ever runs.
CREATE TABLE tasks.tasks (
    id           UUID PRIMARY KEY,
    user_id      UUID NOT NULL,
    title        TEXT NOT NULL,
    description  TEXT NOT NULL DEFAULT '',
    status       tasks.status NOT NULL DEFAULT 'todo',
    priority     tasks.priority NOT NULL,
    due_date     TIMESTAMPTZ,
    completed_at TIMESTAMPTZ,
    created_at   TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at   TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- Every RPC filters by user_id first (tasks are always scoped to their
-- owner); created_at DESC is the default ListTasks sort and its tiebreak.
CREATE INDEX idx_tasks_user_id_created_at ON tasks.tasks (user_id, created_at DESC, id);

-- Backs ListTasks filtered by status and the dashboard's per-status counts.
CREATE INDEX idx_tasks_user_id_status ON tasks.tasks (user_id, status);

-- +goose Down
DROP TABLE tasks.tasks;
DROP TYPE tasks.priority;
DROP TYPE tasks.status;
DROP SCHEMA tasks;
