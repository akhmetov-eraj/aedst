-- +goose Up

-- One row per (task, due date) a TaskDueSoon event was published for. The
-- reminder job inserts it in the same transaction as the outbox row with
-- ON CONFLICT DO NOTHING RETURNING, so a retried job can never publish a second
-- event for the same due date (the "guarded in SQL" rule). Rows go with their
-- task (same schema, so a foreign key is fine).
CREATE TABLE tasks.due_soon_marks (
    task_id   UUID        NOT NULL REFERENCES tasks.tasks (id) ON DELETE CASCADE,
    due_date  TIMESTAMPTZ NOT NULL,
    marked_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (task_id, due_date)
);

-- +goose Down
DROP TABLE tasks.due_soon_marks;
