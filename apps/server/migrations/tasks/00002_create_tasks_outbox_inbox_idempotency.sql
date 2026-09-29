-- +goose Up

-- Transactional outbox: a row is inserted in the same transaction as the
-- domain change that produced it (see internal/platform/outbox), then
-- published asynchronously by a background Forwarder. published_at/poisoned_at
-- NULL means "still owed a publish"; the partial index below is what the
-- Forwarder's FetchUnpublishedBatch scans.
CREATE TABLE tasks.outbox (
    event_id      UUID PRIMARY KEY,
    event_type    TEXT NOT NULL,
    event_version INTEGER NOT NULL,
    occurred_at   TIMESTAMPTZ NOT NULL,
    partition_key UUID NOT NULL,
    traceparent   TEXT NOT NULL DEFAULT '',
    payload       BYTEA NOT NULL,
    published_at  TIMESTAMPTZ,
    attempts      INTEGER NOT NULL DEFAULT 0,
    next_retry_at TIMESTAMPTZ,
    poisoned_at   TIMESTAMPTZ
);

CREATE INDEX idx_tasks_outbox_unpublished ON tasks.outbox (occurred_at)
    WHERE published_at IS NULL AND poisoned_at IS NULL;

-- Inbox: dedup table for events this module's own consumers have already
-- processed (see internal/platform/inbox). Keyed by the producing side's
-- outbox event_id, propagated as the message's idempotency-key header.
CREATE TABLE tasks.inbox (
    event_id     UUID PRIMARY KEY,
    processed_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- Command idempotency: replays a client's retried command instead of
-- re-executing it (see internal/platform/idempotency). request_hash guards
-- against the same key being reused for a different request.
CREATE TABLE tasks.idempotency_keys (
    key          TEXT PRIMARY KEY,
    request_hash TEXT NOT NULL,
    result       BYTEA NOT NULL,
    created_at   TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- +goose Down
DROP TABLE tasks.idempotency_keys;
DROP TABLE tasks.inbox;
DROP TABLE tasks.outbox;
