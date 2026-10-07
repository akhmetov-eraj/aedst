//go:build integration

package postgrestest_test

import (
	"context"
	"testing"

	"github.com/aedst/server/internal/platform/postgrestest"
	"github.com/aedst/server/internal/shared/ids"
)

// The move of outbox/inbox/idempotency into the shared platform schema runs on
// databases that already hold data. Undelivered events must survive the
// upgrade (they are owed to consumers), processed-event markers must survive
// (or redelivered events would be applied twice), and delivered history may be
// left behind.
func TestMigration_MessagingTablesMoveToPlatformWithoutLosingPendingWork(t *testing.T) {
	dsn := postgrestest.NewDatabase(t)
	postgrestest.MigrateTo(t, dsn, "platform", 1)
	postgrestest.MigrateTo(t, dsn, "tasks", 5)
	postgrestest.MigrateTo(t, dsn, "notifications", 1)
	pool := postgrestest.Connect(t, dsn)
	ctx := context.Background()

	pending, delivered, poisoned, processed := ids.New(), ids.New(), ids.New(), ids.New()
	exec := func(sql string, args ...any) {
		t.Helper()
		if _, err := pool.Exec(ctx, sql, args...); err != nil {
			t.Fatalf("%s: %v", sql, err)
		}
	}
	insertOutbox := `INSERT INTO tasks.outbox (event_id, event_type, event_version, occurred_at, partition_key, payload, published_at, poisoned_at)
		VALUES ($1, 'tasks.v1.TaskCompleted', 1, now(), $2, '\x7b7d', $3, $4)`
	exec(insertOutbox, pending.String(), ids.New().String(), nil, nil)
	exec(insertOutbox, delivered.String(), ids.New().String(), "2026-01-01T00:00:00Z", nil)
	exec(insertOutbox, poisoned.String(), ids.New().String(), nil, "2026-01-02T00:00:00Z")
	exec(`INSERT INTO tasks.inbox (event_id) VALUES ($1)`, processed.String())
	exec(`INSERT INTO notifications.inbox (event_id) VALUES ($1)`, processed.String())
	exec(`INSERT INTO tasks.idempotency_keys (key, request_hash, result) VALUES ('k', 'h', '\x00')`)

	postgrestest.Migrate(t, dsn, "tasks", "notifications")

	count := func(query string, args ...any) (n int) {
		t.Helper()
		if err := pool.QueryRow(ctx, query, args...).Scan(&n); err != nil {
			t.Fatalf("%s: %v", query, err)
		}
		return n
	}
	if got := count(`SELECT count(*) FROM platform.outbox WHERE module = 'tasks' AND event_id = $1 AND published_at IS NULL AND poisoned_at IS NULL`, pending.String()); got != 1 {
		t.Errorf("the undelivered event was lost by the migration")
	}
	if got := count(`SELECT count(*) FROM platform.outbox WHERE event_id = $1`, poisoned.String()); got != 1 {
		t.Errorf("the poisoned event (kept for inspection) was lost")
	}
	if got := count(`SELECT count(*) FROM platform.outbox WHERE event_id = $1`, delivered.String()); got != 0 {
		t.Errorf("delivered history should not be copied (%d rows)", got)
	}
	if got := count(`SELECT count(*) FROM platform.inbox WHERE consumer = 'tasks' AND event_id = $1`, processed.String()); got != 1 {
		t.Errorf("the tasks inbox marker was lost")
	}
	if got := count(`SELECT count(*) FROM platform.inbox WHERE consumer = 'notifications.task_completed' AND event_id = $1`, processed.String()); got != 1 {
		t.Errorf("the notifications inbox marker was lost (a redelivered event would be applied twice)")
	}
	for _, table := range []string{"tasks.outbox", "tasks.inbox", "tasks.idempotency_keys", "notifications.inbox"} {
		if got := count(`SELECT count(*) FROM pg_tables WHERE schemaname || '.' || tablename = $1`, table); got != 0 {
			t.Errorf("legacy table %s still exists", table)
		}
	}
}
