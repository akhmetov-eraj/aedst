-- name: CreateCredential :exec
INSERT INTO auth.credentials (user_id, email, password_hash, created_at)
VALUES ($1, $2, $3, $4);

-- name: FindCredentialByEmail :one
SELECT user_id, email, password_hash, created_at
FROM auth.credentials
WHERE email = $1;

-- name: CreateSession :exec
INSERT INTO auth.sessions (id, user_id, device_id, refresh_token_hash, expires_at, created_at)
VALUES ($1, $2, $3, $4, $5, $6);

-- name: FindSessionByID :one
SELECT id, user_id, device_id, refresh_token_hash, expires_at, revoked_at, created_at
FROM auth.sessions
WHERE id = $1;

-- name: RotateSession :exec
UPDATE auth.sessions
SET refresh_token_hash = $2,
    expires_at = $3
WHERE id = $1;

-- name: RevokeSession :exec
UPDATE auth.sessions
SET revoked_at = $2
WHERE id = $1;
