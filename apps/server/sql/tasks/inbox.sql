-- name: IsInboxEventProcessed :one
SELECT EXISTS (SELECT 1 FROM tasks.inbox WHERE event_id = $1);

-- name: MarkInboxEventProcessed :exec
INSERT INTO tasks.inbox (event_id) VALUES ($1)
ON CONFLICT (event_id) DO NOTHING;
