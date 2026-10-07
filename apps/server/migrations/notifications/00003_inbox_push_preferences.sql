-- +goose Up

-- In-app inbox fields used by NotificationsService (notifications.proto).
-- Constant defaults: metadata-only changes, no table rewrite.
ALTER TABLE notifications.notifications ADD COLUMN body TEXT NOT NULL DEFAULT '';
ALTER TABLE notifications.notifications ADD COLUMN link TEXT NOT NULL DEFAULT '';
ALTER TABLE notifications.notifications ADD COLUMN subject_id UUID;

-- Push channels. address is the FCM/APNs device token or the Web Push
-- endpoint URL; (platform, address) identifies one installation, so a token
-- that re-registers (or moves to another account on a shared device) updates
-- its row instead of duplicating it. disabled_at is set when the provider
-- reports the token invalid or the user unregisters.
CREATE TABLE notifications.push_devices (
    id           UUID PRIMARY KEY,
    user_id      UUID        NOT NULL,
    platform     TEXT        NOT NULL CHECK (platform IN ('web_push', 'fcm', 'apns')),
    address      TEXT        NOT NULL,
    p256dh       TEXT        NOT NULL DEFAULT '',
    auth_secret  TEXT        NOT NULL DEFAULT '',
    apns_topic   TEXT        NOT NULL DEFAULT '',
    apns_sandbox BOOLEAN     NOT NULL DEFAULT false,
    device_name  TEXT        NOT NULL DEFAULT '',
    locale       TEXT        NOT NULL DEFAULT '',
    created_at   TIMESTAMPTZ NOT NULL,
    updated_at   TIMESTAMPTZ NOT NULL,
    disabled_at  TIMESTAMPTZ,
    UNIQUE (platform, address)
);

-- Fan-out reads a user's active devices.
CREATE INDEX idx_push_devices_user_active ON notifications.push_devices (user_id)
    WHERE disabled_at IS NULL;
-- Retention purges long-disabled devices.
CREATE INDEX idx_push_devices_disabled_at ON notifications.push_devices (disabled_at)
    WHERE disabled_at IS NOT NULL;

-- Per-user switches; a missing row means "everything on" (domain.DefaultPreferences).
CREATE TABLE notifications.preferences (
    user_id        UUID PRIMARY KEY,
    push_enabled   BOOLEAN     NOT NULL DEFAULT true,
    task_completed BOOLEAN     NOT NULL DEFAULT true,
    task_due_soon  BOOLEAN     NOT NULL DEFAULT true,
    media_ready    BOOLEAN     NOT NULL DEFAULT true,
    updated_at     TIMESTAMPTZ NOT NULL
);

-- +goose Down
DROP TABLE notifications.preferences;
DROP TABLE notifications.push_devices;
ALTER TABLE notifications.notifications DROP COLUMN subject_id;
ALTER TABLE notifications.notifications DROP COLUMN link;
ALTER TABLE notifications.notifications DROP COLUMN body;
