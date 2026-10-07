-- name: CreateCredential :exec
INSERT INTO auth.credentials (user_id, email, password_hash, created_at)
VALUES ($1, $2, $3, $4);

-- name: FindCredentialByEmail :one
SELECT user_id, email, password_hash, created_at
FROM auth.credentials
WHERE email = $1;

-- name: FindCredentialByUserID :one
SELECT user_id, email, password_hash, created_at
FROM auth.credentials
WHERE user_id = $1;

-- name: CreateSession :exec
INSERT INTO auth.sessions (id, user_id, device_id, refresh_token_hash, expires_at, created_at)
VALUES ($1, $2, $3, $4, $5, $6);

-- name: FindSessionByID :one
SELECT id, user_id, device_id, refresh_token_hash, expires_at, revoked_at, created_at
FROM auth.sessions
WHERE id = $1;

-- name: RotateSession :execrows
-- Compare-and-swap: only the holder of the current refresh token, on a live
-- session, can rotate it. Zero rows affected means a stale/replayed token.
UPDATE auth.sessions
SET refresh_token_hash = sqlc.arg(new_refresh_token_hash),
    expires_at = sqlc.arg(expires_at)
WHERE id = sqlc.arg(id)
  AND refresh_token_hash = sqlc.arg(old_refresh_token_hash)
  AND revoked_at IS NULL
  AND expires_at > now();

-- name: UpdateCredentialPasswordHash :exec
UPDATE auth.credentials
SET password_hash = $2
WHERE user_id = $1;

-- name: RevokeSession :exec
UPDATE auth.sessions
SET revoked_at = $2
WHERE id = $1
  AND revoked_at IS NULL;

-- name: PurgeSessions :execrows
-- Retention: sessions that expired or were revoked before the cutoff can never
-- authenticate again and only cost space. Batched (purge_limit) so a large
-- backlog never becomes one long DELETE.
DELETE FROM auth.sessions
WHERE id IN (
    SELECT s.id FROM auth.sessions s
    WHERE s.expires_at < sqlc.arg(cutoff) OR s.revoked_at < sqlc.arg(cutoff)
    LIMIT sqlc.arg(purge_limit)
);

-- Operator directory (admin module via auth/api.Directory). Keyset page,
-- newest first; search is a case-insensitive substring of the email with LIKE
-- metacharacters escaped by the caller.
-- name: ListCredentialsPage :many
SELECT user_id, email, created_at
FROM auth.credentials
WHERE (sqlc.arg(search)::text = '' OR email ILIKE '%' || sqlc.arg(search)::text || '%')
  AND (created_at, user_id) < (sqlc.arg(cursor_key)::timestamptz, sqlc.arg(cursor_id)::uuid)
ORDER BY created_at DESC, user_id DESC
LIMIT sqlc.arg(page_limit);

-- name: CountCredentials :one
SELECT count(*) FROM auth.credentials;
