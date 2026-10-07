-- +goose Up

-- The outbox, inbox and idempotency tables now live in the shared `platform`
-- schema (migrations/platform must be applied first). Carry over everything
-- that is still owed: undelivered outbox rows (so no event is lost by the
-- upgrade), the inbox (so redelivered events stay deduplicated) and recent
-- idempotency keys.
INSERT INTO platform.outbox (event_id, module, event_type, event_version, occurred_at, partition_key,
                             traceparent, payload, published_at, attempts, next_retry_at, poisoned_at)
SELECT event_id, 'tasks', event_type, event_version, occurred_at, partition_key,
       traceparent, payload, published_at, attempts, next_retry_at, poisoned_at
FROM tasks.outbox
WHERE published_at IS NULL OR poisoned_at IS NOT NULL
ON CONFLICT (event_id) DO NOTHING;

INSERT INTO platform.inbox (consumer, event_id, processed_at)
SELECT 'tasks', event_id, processed_at FROM tasks.inbox
ON CONFLICT DO NOTHING;

-- Old keys were global; the new ones are scoped to a user. A key carried over
-- without its user could never match again, and keys are only meaningful for a
-- client's retry window, so they are not migrated.

-- squawk-ignore ban-drop-table
DROP TABLE tasks.idempotency_keys;
-- squawk-ignore ban-drop-table
DROP TABLE tasks.inbox;
-- squawk-ignore ban-drop-table
DROP TABLE tasks.outbox;

-- +goose Down
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
CREATE INDEX idx_tasks_outbox_published ON tasks.outbox (published_at)
    WHERE published_at IS NOT NULL;
CREATE TABLE tasks.inbox (
    event_id     UUID PRIMARY KEY,
    processed_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE TABLE tasks.idempotency_keys (
    key          TEXT PRIMARY KEY,
    request_hash TEXT NOT NULL,
    result       BYTEA NOT NULL,
    created_at   TIMESTAMPTZ NOT NULL DEFAULT now()
);
INSERT INTO tasks.outbox (event_id, event_type, event_version, occurred_at, partition_key,
                          traceparent, payload, published_at, attempts, next_retry_at, poisoned_at)
SELECT event_id, event_type, event_version, occurred_at, partition_key,
       traceparent, payload, published_at, attempts, next_retry_at, poisoned_at
FROM platform.outbox WHERE module = 'tasks';
INSERT INTO tasks.inbox (event_id, processed_at)
SELECT event_id, processed_at FROM platform.inbox WHERE consumer = 'tasks';
