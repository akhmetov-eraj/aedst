//go:build integration

package outbox_test

import (
	"context"
	"sync"
	"testing"
	"time"

	"github.com/jackc/pgx/v5"

	"github.com/aedst/server/internal/platform/database"
	"github.com/aedst/server/internal/platform/outbox"
	"github.com/aedst/server/internal/platform/postgrestest"
	"github.com/aedst/server/internal/shared/ids"
	sharedtime "github.com/aedst/server/internal/shared/time"
)

const testEventType = "test.v1.Event"

// Two relays (two server replicas) draining the same outbox must never
// deliver a row twice: Claim leases rows with FOR UPDATE SKIP LOCKED.
func TestOutboxRepository_ConcurrentClaimsAreDisjoint(t *testing.T) {
	pool := postgrestest.NewPool(t)
	outboxRepo := outbox.NewPostgresRepository(pool, "tasks")
	ctx := context.Background()

	const total = 200
	clock := sharedtime.Real{}
	for range total {
		row := outbox.NewRow(ctx, clock, testEventType, 1, ids.New(), []byte(`{}`))
		if err := database.WithTx(ctx, pool, func(ctx context.Context, tx pgx.Tx) error { return outboxRepo.Insert(ctx, tx, row) }); err != nil {
			t.Fatalf("Insert() error = %v", err)
		}
	}

	claimed := make(chan ids.ID, total*2)
	var wg sync.WaitGroup
	for range 4 {
		wg.Go(func() {
			for {
				rows, err := outboxRepo.Claim(ctx, 7, time.Minute)
				if err != nil {
					t.Errorf("Claim() error = %v", err)
					return
				}
				if len(rows) == 0 {
					return
				}
				for _, r := range rows {
					claimed <- r.EventID
				}
			}
		})
	}
	wg.Wait()
	close(claimed)

	seen := map[ids.ID]bool{}
	for id := range claimed {
		if seen[id] {
			t.Fatalf("event %s claimed by two relays", id)
		}
		seen[id] = true
	}
	if len(seen) != total {
		t.Fatalf("claimed %d events, want %d", len(seen), total)
	}
}

func TestOutboxRepository_LeaseExpiryRetryAndPoison(t *testing.T) {
	pool := postgrestest.NewPool(t)
	outboxRepo := outbox.NewPostgresRepository(pool, "tasks")
	ctx := context.Background()

	row := outbox.NewRow(ctx, sharedtime.Real{}, testEventType, 1, ids.New(), []byte(`{}`))
	if err := database.WithTx(ctx, pool, func(ctx context.Context, tx pgx.Tx) error { return outboxRepo.Insert(ctx, tx, row) }); err != nil {
		t.Fatalf("Insert() error = %v", err)
	}

	first, err := outboxRepo.Claim(ctx, 10, time.Hour)
	if err != nil || len(first) != 1 || first[0].Attempts != 1 {
		t.Fatalf("Claim() = %+v, %v; want one row at attempt 1", first, err)
	}
	if again, _ := outboxRepo.Claim(ctx, 10, time.Hour); len(again) != 0 {
		t.Fatalf("leased row claimed again: %+v", again)
	}

	// A retry in the past makes it immediately claimable, with attempts kept.
	if err := outboxRepo.Retry(ctx, row.EventID, time.Now().Add(-time.Second)); err != nil {
		t.Fatalf("Retry() error = %v", err)
	}
	second, err := outboxRepo.Claim(ctx, 10, time.Hour)
	if err != nil || len(second) != 1 || second[0].Attempts != 2 {
		t.Fatalf("Claim() after Retry = %+v, %v; want one row at attempt 2", second, err)
	}

	backlog, err := outboxRepo.Backlog(ctx)
	if err != nil || backlog.Pending != 1 || backlog.Oldest.IsZero() {
		t.Fatalf("Backlog() = %+v, %v; want 1 pending with an oldest time", backlog, err)
	}

	if err := outboxRepo.MarkPoison(ctx, row.EventID); err != nil {
		t.Fatalf("MarkPoison() error = %v", err)
	}
	if err := outboxRepo.Retry(ctx, row.EventID, time.Now().Add(-time.Second)); err != nil {
		t.Fatalf("Retry() error = %v", err)
	}
	if rows, _ := outboxRepo.Claim(ctx, 10, time.Hour); len(rows) != 0 {
		t.Fatalf("poisoned row claimed: %+v", rows)
	}
	if backlog, _ := outboxRepo.Backlog(ctx); backlog.Pending != 0 {
		t.Fatalf("poisoned row still counted as pending: %+v", backlog)
	}
}

// TestRepository_Complete_NotFoundWritesNothing proves atomicity:
// if the transaction fails, neither the status change nor the outbox row
// survives.
