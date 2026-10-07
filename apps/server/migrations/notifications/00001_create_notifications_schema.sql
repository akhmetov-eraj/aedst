-- +goose Up
CREATE SCHEMA IF NOT EXISTS notifications;

-- The notifications module is a pure consumer: every row is a projection of
-- an integration event published by another module (see
-- docs/content/events.md). user_id has no FK to auth.* — modules never
-- reference another module's schema, not even by constraint (database.md).
CREATE TABLE notifications.notifications (
    id              UUID PRIMARY KEY,
    user_id         UUID NOT NULL,
    kind            TEXT NOT NULL,
    title           TEXT NOT NULL,
    -- The outbox event this notification was derived from. UNIQUE makes the
    -- projection idempotent even if the inbox were bypassed (defense in depth).
    source_event_id UUID NOT NULL UNIQUE,
    created_at      TIMESTAMPTZ NOT NULL,
    read_at         TIMESTAMPTZ
);

-- Keyset pagination of a user's feed, newest first (internal/shared/pagination):
-- owner id leads (the shard key, ADR-0010), then the sort order.
CREATE INDEX idx_notifications_user_created_keyset
    ON notifications.notifications (user_id, created_at, id);

-- Inbox: dedup of events this module has already applied
-- (internal/platform/inbox), keyed by the producing outbox row's event_id.
CREATE TABLE notifications.inbox (
    event_id     UUID PRIMARY KEY,
    processed_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- +goose Down
DROP TABLE notifications.inbox;
DROP TABLE notifications.notifications;
DROP SCHEMA notifications;
