-- +goose Up

-- The inbox now lives in the shared `platform` schema (migrations/platform must
-- be applied first), keyed by (consumer, event_id). Carry over what has been
-- applied so redelivered events stay deduplicated.
INSERT INTO platform.inbox (consumer, event_id, processed_at)
SELECT 'notifications.task_completed', event_id, processed_at FROM notifications.inbox
ON CONFLICT DO NOTHING;

-- squawk-ignore ban-drop-table
DROP TABLE notifications.inbox;

-- +goose Down
CREATE TABLE notifications.inbox (
    event_id     UUID PRIMARY KEY,
    processed_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
INSERT INTO notifications.inbox (event_id, processed_at)
SELECT event_id, processed_at FROM platform.inbox WHERE consumer = 'notifications.task_completed';
