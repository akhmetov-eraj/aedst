-- +goose no transaction
-- +goose Up

-- Unread-only listing and the unread badge count: a partial index holds only
-- unread rows, so both stay cheap however large the read history grows.
CREATE INDEX CONCURRENTLY IF NOT EXISTS idx_notifications_user_unread_keyset
    ON notifications.notifications (user_id, created_at, id)
    WHERE read_at IS NULL;

-- Retention purges old notifications by age (AEDST_RETENTION_NOTIFICATION_DAYS).
CREATE INDEX CONCURRENTLY IF NOT EXISTS idx_notifications_created_at
    ON notifications.notifications (created_at);

-- +goose Down
DROP INDEX CONCURRENTLY IF EXISTS notifications.idx_notifications_created_at;
DROP INDEX CONCURRENTLY IF EXISTS notifications.idx_notifications_user_unread_keyset;
