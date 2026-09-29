//go:build integration

// Integration tests against a real PostgreSQL container — see
// docs/database.md and `make integration-test`. Run with
// `go test -tags=integration ./internal/modules/tasks/...`; skipped by the
// default `go test ./...` (no build tag) and by `make test`.
package postgres_test

import (
	"context"
	"database/sql"
	"testing"
	"time"

	"github.com/jackc/pgx/v5/pgxpool"
	_ "github.com/jackc/pgx/v5/stdlib" // registers the "pgx" database/sql driver for goose's migration runner
	"github.com/pressly/goose/v3"
	tc "github.com/testcontainers/testcontainers-go"
	tcpostgres "github.com/testcontainers/testcontainers-go/modules/postgres"
	"github.com/testcontainers/testcontainers-go/wait"

	"github.com/aedst/server/internal/modules/tasks/application"
	"github.com/aedst/server/internal/modules/tasks/domain"
	"github.com/aedst/server/internal/modules/tasks/infrastructure/postgres"
	sharederrors "github.com/aedst/server/internal/shared/errors"
	"github.com/aedst/server/internal/shared/ids"
	sharedtime "github.com/aedst/server/internal/shared/time"
)

// newRepository wires a Repository with real (but per-test-pool) outbox and
// idempotency adapters, matching how cmd/server constructs one.
func newRepository(pool *pgxpool.Pool) *postgres.Repository {
	return postgres.NewRepository(pool, postgres.NewOutboxRepository(pool), postgres.NewIdempotencyStore(), sharedtime.Real{})
}

// newTestPool starts a real Postgres container, applies the tasks module's
// goose migrations against it (the same migrations/tasks/ files `make
// migrate-up` runs), and returns a pool scoped to the test's lifetime.
func newTestPool(t *testing.T) *pgxpool.Pool {
	t.Helper()
	ctx := context.Background()

	container, err := tcpostgres.Run(ctx, "postgres:18",
		tcpostgres.WithDatabase("aedst_test"),
		tcpostgres.WithUsername("aedst"),
		tcpostgres.WithPassword("aedst"),
		tc.WithWaitStrategy(wait.ForListeningPort("5432/tcp").WithStartupTimeout(30*time.Second)),
	)
	if err != nil {
		t.Fatalf("start postgres container: %v", err)
	}
	t.Cleanup(func() { _ = container.Terminate(context.Background()) })

	dsn, err := container.ConnectionString(ctx, "sslmode=disable")
	if err != nil {
		t.Fatalf("container connection string: %v", err)
	}

	// goose's migration runner wants database/sql, unlike the app's own
	// pgx/v5 pool below — the same split every `goose -dir migrations/...`
	// CLI invocation has relative to the app's runtime pgxpool.Pool.
	sqlDB, err := sql.Open("pgx", dsn)
	if err != nil {
		t.Fatalf("open migration connection: %v", err)
	}
	defer sqlDB.Close()

	if err := goose.SetDialect("postgres"); err != nil {
		t.Fatalf("goose.SetDialect: %v", err)
	}
	if err := goose.Up(sqlDB, "../../../../migrations/tasks"); err != nil {
		t.Fatalf("apply tasks migrations: %v", err)
	}

	pool, err := pgxpool.New(ctx, dsn)
	if err != nil {
		t.Fatalf("open pgxpool: %v", err)
	}
	t.Cleanup(pool.Close)

	return pool
}

func TestRepository_CreateAndFind(t *testing.T) {
	pool := newTestPool(t)
	repo := newRepository(pool)
	ctx := context.Background()

	userID := ids.New()
	now := time.Now().UTC().Truncate(time.Second)
	created, err := repo.Create(ctx, domain.Task{
		ID:        ids.New(),
		UserID:    userID,
		Title:     "Write integration test",
		Status:    domain.StatusTodo,
		Priority:  domain.PriorityHigh,
		CreatedAt: now,
		UpdatedAt: now,
	}, "")
	if err != nil {
		t.Fatalf("Create() error = %v", err)
	}

	found, err := repo.FindByIDAndUserID(ctx, created.ID, userID)
	if err != nil {
		t.Fatalf("FindByIDAndUserID() error = %v", err)
	}
	if found.Title != "Write integration test" {
		t.Errorf("FindByIDAndUserID() title = %q, want %q", found.Title, "Write integration test")
	}
}

func TestRepository_FindByIDAndUserID_CrossUserIsNotFound(t *testing.T) {
	pool := newTestPool(t)
	repo := newRepository(pool)
	ctx := context.Background()

	owner := ids.New()
	now := time.Now().UTC()
	task, err := repo.Create(ctx, domain.Task{
		ID: ids.New(), UserID: owner, Title: "Owner's task",
		Status: domain.StatusTodo, Priority: domain.PriorityLow,
		CreatedAt: now, UpdatedAt: now,
	}, "")
	if err != nil {
		t.Fatalf("Create() error = %v", err)
	}

	_, err = repo.FindByIDAndUserID(ctx, task.ID, ids.New())
	if sharederrors.KindOf(err) != sharederrors.KindNotFound {
		t.Fatalf("cross-user FindByIDAndUserID() = %v, want KindNotFound", err)
	}
}

func TestRepository_ListAndStats(t *testing.T) {
	pool := newTestPool(t)
	repo := newRepository(pool)
	ctx := context.Background()

	userID := ids.New()
	now := time.Now().UTC()
	seed := []domain.Task{
		{ID: ids.New(), UserID: userID, Title: "todo-1", Status: domain.StatusTodo, Priority: domain.PriorityLow, CreatedAt: now, UpdatedAt: now},
		{ID: ids.New(), UserID: userID, Title: "in-progress-1", Status: domain.StatusTodo, Priority: domain.PriorityMedium, CreatedAt: now.Add(time.Second), UpdatedAt: now},
	}
	for i := range seed {
		created, err := repo.Create(ctx, seed[i], "")
		if err != nil {
			t.Fatalf("seed Create() #%d error = %v", i, err)
		}
		seed[i] = created
	}

	completed, err := repo.SetStatus(ctx, seed[1].ID, userID, domain.StatusDone, &now, now)
	if err != nil {
		t.Fatalf("SetStatus() error = %v", err)
	}
	if completed.Status != domain.StatusDone {
		t.Errorf("SetStatus() status = %v, want StatusDone", completed.Status)
	}

	tasks, total, err := repo.List(ctx, application.ListParams{
		UserID: userID, SortBy: "created_at", SortDirection: "desc", PageSize: 10,
	})
	if err != nil {
		t.Fatalf("List() error = %v", err)
	}
	if total != 2 || len(tasks) != 2 {
		t.Fatalf("List() = %d tasks (total %d), want 2", len(tasks), total)
	}
	if tasks[0].Title != "in-progress-1" {
		t.Errorf("List() sort order = %q first, want most-recently-created first", tasks[0].Title)
	}

	stats, err := repo.Stats(ctx, userID)
	if err != nil {
		t.Fatalf("Stats() error = %v", err)
	}
	if stats.Total != 2 || stats.Todo != 1 || stats.Done != 1 {
		t.Errorf("Stats() = %+v, want {Total:2 Todo:1 Done:1 ...}", stats)
	}
}

func TestRepository_Delete(t *testing.T) {
	pool := newTestPool(t)
	repo := newRepository(pool)
	ctx := context.Background()

	userID := ids.New()
	now := time.Now().UTC()
	task, err := repo.Create(ctx, domain.Task{
		ID: ids.New(), UserID: userID, Title: "to delete",
		Status: domain.StatusTodo, Priority: domain.PriorityLow,
		CreatedAt: now, UpdatedAt: now,
	}, "")
	if err != nil {
		t.Fatalf("Create() error = %v", err)
	}

	if err := repo.Delete(ctx, task.ID, userID); err != nil {
		t.Fatalf("Delete() error = %v", err)
	}

	_, err = repo.FindByIDAndUserID(ctx, task.ID, userID)
	if sharederrors.KindOf(err) != sharederrors.KindNotFound {
		t.Fatalf("FindByIDAndUserID() after delete = %v, want KindNotFound", err)
	}

	if err := repo.Delete(ctx, task.ID, userID); sharederrors.KindOf(err) != sharederrors.KindNotFound {
		t.Fatalf("second Delete() = %v, want KindNotFound", err)
	}
}

// TestRepository_Create_IdempotentReplay proves Create's idempotency
// integration: the same key with the same input returns the original task
// (no duplicate row), and a different input under the same key is rejected
// as a conflict — see internal/platform/idempotency.
func TestRepository_Create_IdempotentReplay(t *testing.T) {
	pool := newTestPool(t)
	repo := newRepository(pool)
	ctx := context.Background()

	userID := ids.New()
	now := time.Now().UTC().Truncate(time.Second)
	task := domain.Task{
		ID: ids.New(), UserID: userID, Title: "Idempotent task",
		Status: domain.StatusTodo, Priority: domain.PriorityLow,
		CreatedAt: now, UpdatedAt: now,
	}

	first, err := repo.Create(ctx, task, "retry-key-1")
	if err != nil {
		t.Fatalf("first Create() error = %v", err)
	}

	// A retry generates a fresh domain.Task (new ID/CreatedAt, as
	// application.Service.CreateTask does) but carries the same
	// idempotency key and the same user-facing fields — it must replay the
	// original result, not create a second task.
	retry := task
	retry.ID = ids.New()
	second, err := repo.Create(ctx, retry, "retry-key-1")
	if err != nil {
		t.Fatalf("replayed Create() error = %v", err)
	}
	if second.ID != first.ID {
		t.Fatalf("replayed Create() returned a different task: got %s, want %s", second.ID, first.ID)
	}

	_, total, err := repo.List(ctx, application.ListParams{UserID: userID, SortBy: "created_at", SortDirection: "desc", PageSize: 10})
	if err != nil {
		t.Fatalf("List() error = %v", err)
	}
	if total != 1 {
		t.Fatalf("List() total = %d, want 1 (replay must not create a duplicate row)", total)
	}

	conflicting := task
	conflicting.ID = ids.New()
	conflicting.Title = "Different title"
	if _, err := repo.Create(ctx, conflicting, "retry-key-1"); sharederrors.KindOf(err) != sharederrors.KindConflict {
		t.Fatalf("Create() with reused key and different payload = %v, want KindConflict", err)
	}
}

// TestRepository_Complete_WritesOutboxRow proves Complete's outbox
// integration: completing a task writes an unpublished tasks.outbox row in
// the same transaction as the status change — see internal/platform/outbox.
func TestRepository_Complete_WritesOutboxRow(t *testing.T) {
	pool := newTestPool(t)
	repo := newRepository(pool)
	outboxRepo := postgres.NewOutboxRepository(pool)
	ctx := context.Background()

	userID := ids.New()
	now := time.Now().UTC().Truncate(time.Second)
	created, err := repo.Create(ctx, domain.Task{
		ID: ids.New(), UserID: userID, Title: "Complete me",
		Status: domain.StatusTodo, Priority: domain.PriorityLow,
		CreatedAt: now, UpdatedAt: now,
	}, "")
	if err != nil {
		t.Fatalf("Create() error = %v", err)
	}

	completed, err := repo.Complete(ctx, created.ID, userID, now, now)
	if err != nil {
		t.Fatalf("Complete() error = %v", err)
	}
	if completed.Status != domain.StatusDone {
		t.Fatalf("Complete() status = %v, want StatusDone", completed.Status)
	}

	rows, err := outboxRepo.FetchUnpublishedBatch(ctx, 10)
	if err != nil {
		t.Fatalf("FetchUnpublishedBatch() error = %v", err)
	}
	if len(rows) != 1 {
		t.Fatalf("FetchUnpublishedBatch() = %d rows, want 1", len(rows))
	}
	if rows[0].EventType != postgres.TaskCompletedEventType {
		t.Errorf("outbox row event_type = %q, want %q", rows[0].EventType, postgres.TaskCompletedEventType)
	}
	if rows[0].PartitionKey != created.ID {
		t.Errorf("outbox row partition_key = %s, want task id %s", rows[0].PartitionKey, created.ID)
	}
}
