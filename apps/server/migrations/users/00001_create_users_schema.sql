-- +goose Up
CREATE SCHEMA IF NOT EXISTS users;

CREATE TABLE users.users (
    id         UUID PRIMARY KEY,
    email      TEXT NOT NULL UNIQUE,
    roles      TEXT[] NOT NULL DEFAULT '{}',
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- +goose Down
DROP TABLE users.users;
DROP SCHEMA users;
