-- name: CreateUser :exec
INSERT INTO users.users (id, email, roles, created_at)
VALUES ($1, $2, $3, $4);

-- name: FindUserByID :one
SELECT id, email, roles, created_at
FROM users.users
WHERE id = $1;

-- name: FindUserByEmail :one
SELECT id, email, roles, created_at
FROM users.users
WHERE email = $1;
