-- +goose Up
CREATE SCHEMA IF NOT EXISTS profile;

CREATE TABLE profile.profiles (
    user_id      UUID PRIMARY KEY,
    display_name TEXT NOT NULL DEFAULT '',
    bio          TEXT NOT NULL DEFAULT '',
    avatar_url   TEXT NOT NULL DEFAULT '',
    updated_at   TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- +goose Down
DROP TABLE profile.profiles;
DROP SCHEMA profile;
