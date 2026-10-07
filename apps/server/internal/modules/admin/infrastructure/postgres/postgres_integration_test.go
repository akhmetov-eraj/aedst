//go:build integration

package postgres_test

import (
	"context"
	"errors"
	"testing"
	"time"

	"github.com/jackc/pgx/v5"

	"github.com/aedst/server/internal/modules/admin/domain"
	"github.com/aedst/server/internal/modules/admin/infrastructure/postgres"
	"github.com/aedst/server/internal/platform/postgrestest"
	"github.com/aedst/server/internal/shared/ids"
	sharedtime "github.com/aedst/server/internal/shared/time"
)

func TestAuditLog_AtomicWithTheActionAndPaged(t *testing.T) {
	pool := postgrestest.NewPool(t, "admin")
	repo := postgres.NewRepository(pool, sharedtime.Real{})
	ctx := context.Background()
	actor := domain.Actor{UserID: ids.New(), Subject: "op@example.com", Issuer: "https://idp"}

	for i := range 3 {
		err := repo.Run(ctx, func(ctx context.Context, tx pgx.Tx) error {
			return repo.InsertTx(ctx, tx, domain.AuditEntry{
				ID: ids.New(), Actor: actor, Action: domain.ActionDeleteTask, TargetKind: domain.TargetTask,
				TargetID: ids.New().String(), Reason: "cleanup", Metadata: map[string]any{"i": i},
				CreatedAt: time.Now().Add(time.Duration(i) * time.Millisecond),
			})
		})
		if err != nil {
			t.Fatal(err)
		}
	}
	// The mutation fails after the entry was written: the entry rolls back with it.
	boom := errors.New("mutation failed")
	err := repo.Run(ctx, func(ctx context.Context, tx pgx.Tx) error {
		if err := repo.InsertTx(ctx, tx, domain.AuditEntry{ID: ids.New(), Actor: actor, Action: "x", TargetKind: "y", CreatedAt: time.Now()}); err != nil {
			return err
		}
		return boom
	})
	if !errors.Is(err, boom) {
		t.Fatalf("Run = %v", err)
	}

	first, next, err := repo.List(ctx, 2, nil)
	if err != nil || len(first) != 2 || next == nil || first[0].Metadata["i"] != float64(2) || first[0].Actor != actor {
		t.Fatalf("first page = %+v, %v, %v", first, next, err)
	}
	rest, next, err := repo.List(ctx, 2, next)
	if err != nil || len(rest) != 1 || next != nil {
		t.Fatalf("second page = %+v, %v", rest, err)
	}

	if lag, err := repo.OutboxLag(ctx); err != nil || lag != 0 {
		t.Errorf("OutboxLag = %v, %v", lag, err)
	}
	if n, err := repo.PendingJobs(ctx); err != nil || n != 0 {
		t.Errorf("PendingJobs = %d, %v", n, err)
	}
	if n, err := repo.Purge(ctx, time.Now().Add(time.Hour), 100); err != nil || n != 3 {
		t.Errorf("Purge = %d, %v", n, err)
	}
}
