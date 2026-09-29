-- name: GetIdempotencyKey :one
SELECT result, request_hash FROM tasks.idempotency_keys WHERE key = $1;

-- name: SaveIdempotencyKey :exec
INSERT INTO tasks.idempotency_keys (key, request_hash, result)
VALUES ($1, $2, $3)
ON CONFLICT (key) DO NOTHING;
