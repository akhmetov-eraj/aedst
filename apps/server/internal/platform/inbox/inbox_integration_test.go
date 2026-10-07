//go:build integration

package inbox_test

import (
	"context"
	"errors"
	"log/slog"
	"testing"
	"time"

	"github.com/jackc/pgx/v5"

	"github.com/aedst/server/internal/platform/inbox"
	"github.com/aedst/server/internal/platform/postgrestest"
	"github.com/aedst/server/internal/shared/ids"
)

func TestConsumer_AppliesEachEventOncePerConsumer(t *testing.T) {
	pool := postgrestest.NewPool(t)
	ctx := context.Background()
	logger := slog.New(slog.DiscardHandler)

	var first, second int
	a := inbox.NewConsumer("a", pool, func(context.Context, pgx.Tx, ids.ID, []byte) error { first++; return nil }, logger)
	b := inbox.NewConsumer("b", pool, func(context.Context, pgx.Tx, ids.ID, []byte) error { second++; return nil }, logger)

	event := ids.New()
	for range 3 {
		if err := a.Process(ctx, event, nil); err != nil {
			t.Fatalf("a.Process: %v", err)
		}
	}
	if err := b.Process(ctx, event, nil); err != nil {
		t.Fatalf("b.Process: %v", err)
	}
	if first != 1 || second != 1 {
		t.Fatalf("handler runs = a:%d b:%d, want 1 each (dedupe is per consumer, not per event)", first, second)
	}
}

func TestConsumer_FailedHandlerLeavesEventRedeliverable(t *testing.T) {
	pool := postgrestest.NewPool(t)
	ctx := context.Background()

	calls := 0
	consumer := inbox.NewConsumer("flaky", pool, func(context.Context, pgx.Tx, ids.ID, []byte) error {
		calls++
		if calls == 1 {
			return errors.New("boom")
		}
		return nil
	}, slog.New(slog.DiscardHandler))

	event := ids.New()
	if err := consumer.Process(ctx, event, nil); err == nil {
		t.Fatal("Process succeeded although the handler failed")
	}
	if err := consumer.Process(ctx, event, nil); err != nil {
		t.Fatalf("redelivery: %v", err)
	}
	if calls != 2 {
		t.Fatalf("handler ran %d times, want 2 (the failure must not be recorded as processed)", calls)
	}
}

func TestPurge_DeletesOnlyOldMarkers(t *testing.T) {
	pool := postgrestest.NewPool(t)
	ctx := context.Background()
	consumer := inbox.NewConsumer("purge", pool, func(context.Context, pgx.Tx, ids.ID, []byte) error { return nil }, slog.New(slog.DiscardHandler))
	if err := consumer.Process(ctx, ids.New(), nil); err != nil {
		t.Fatalf("Process: %v", err)
	}

	if n, err := inbox.Purge(ctx, pool, time.Now().Add(-time.Hour), 100); err != nil || n != 0 {
		t.Fatalf("Purge(recent cutoff) = %d, %v; want 0", n, err)
	}
	if n, err := inbox.Purge(ctx, pool, time.Now().Add(time.Hour), 100); err != nil || n != 1 {
		t.Fatalf("Purge(future cutoff) = %d, %v; want 1", n, err)
	}
}
