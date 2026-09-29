-- name: InsertOutboxRow :exec
INSERT INTO tasks.outbox (event_id, event_type, event_version, occurred_at, partition_key, traceparent, payload)
VALUES ($1, $2, $3, $4, $5, $6, $7);

-- name: FetchUnpublishedOutboxRows :many
SELECT event_id, event_type, event_version, occurred_at, partition_key, traceparent, payload,
       published_at, attempts, next_retry_at, poisoned_at
FROM tasks.outbox
WHERE published_at IS NULL
  AND poisoned_at IS NULL
  AND (next_retry_at IS NULL OR next_retry_at <= now())
ORDER BY occurred_at
LIMIT $1;

-- name: MarkOutboxRowsPublished :exec
UPDATE tasks.outbox
SET published_at = now()
WHERE event_id = ANY (sqlc.arg(event_ids)::uuid[]);

-- name: IncrementOutboxAttempts :exec
UPDATE tasks.outbox
SET attempts = attempts + 1, next_retry_at = $2
WHERE event_id = $1;

-- name: MarkOutboxRowPoisoned :exec
UPDATE tasks.outbox
SET poisoned_at = now()
WHERE event_id = $1;
