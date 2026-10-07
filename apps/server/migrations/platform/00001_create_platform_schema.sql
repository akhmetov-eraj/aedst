-- +goose Up

-- Messaging and request-deduplication tables shared by every module. They are
-- owned by internal/platform (outbox, inbox, idempotency) and carry the owning
-- module / consumer / scope as a column, so a new module needs no tables and no
-- repository code of its own for them. Rows still commit in the SAME
-- transaction as the module's domain write (one database), and a module
-- extracted into its own service moves its rows by `WHERE module = ...`.
CREATE SCHEMA IF NOT EXISTS platform;

-- Transactional outbox (see internal/platform/outbox). published_at /
-- poisoned_at NULL means "still owed a delivery".
CREATE TABLE platform.outbox (
    event_id      UUID PRIMARY KEY,
    module        TEXT        NOT NULL,
    event_type    TEXT        NOT NULL,
    event_version INTEGER     NOT NULL,
    occurred_at   TIMESTAMPTZ NOT NULL,
    partition_key UUID        NOT NULL,
    traceparent   TEXT        NOT NULL DEFAULT '',
    payload       BYTEA       NOT NULL,
    published_at  TIMESTAMPTZ,
    attempts      INTEGER     NOT NULL DEFAULT 0,
    next_retry_at TIMESTAMPTZ,
    poisoned_at   TIMESTAMPTZ
);

-- Backs the relay's claim (per module, oldest first).
CREATE INDEX idx_platform_outbox_unpublished ON platform.outbox (module, occurred_at)
    WHERE published_at IS NULL AND poisoned_at IS NULL;

-- Backs the relay's periodic purge of delivered rows.
CREATE INDEX idx_platform_outbox_published ON platform.outbox (module, published_at)
    WHERE published_at IS NOT NULL;

-- Inbox: which events a consumer has already applied (see internal/platform/inbox).
-- Keyed by (consumer, event_id) so several consumers of one event dedupe
-- independently.
CREATE TABLE platform.inbox (
    consumer     TEXT        NOT NULL,
    event_id     UUID        NOT NULL,
    processed_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (consumer, event_id)
);

CREATE INDEX idx_platform_inbox_processed_at ON platform.inbox (processed_at);

-- Command idempotency (see internal/platform/idempotency). scope namespaces the
-- key (module + caller), so two users can never collide on, or probe, each
-- other's keys. request_hash guards against key reuse for a different request.
CREATE TABLE platform.idempotency_keys (
    scope        TEXT        NOT NULL,
    key          TEXT        NOT NULL,
    request_hash TEXT        NOT NULL,
    result       BYTEA       NOT NULL,
    created_at   TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (scope, key)
);

CREATE INDEX idx_platform_idempotency_created_at ON platform.idempotency_keys (created_at);

-- +goose Down
DROP TABLE platform.idempotency_keys;
DROP TABLE platform.inbox;
DROP TABLE platform.outbox;
DROP SCHEMA platform;
