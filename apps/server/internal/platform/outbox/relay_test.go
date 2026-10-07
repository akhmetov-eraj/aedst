package outbox_test

import (
	"context"
	"errors"
	"log/slog"
	"sync"
	"sync/atomic"
	"testing"
	"time"

	"github.com/jackc/pgx/v5"

	"github.com/aedst/server/internal/platform/messaging"
	"github.com/aedst/server/internal/platform/outbox"
	"github.com/aedst/server/internal/shared/ids"
	sharedtime "github.com/aedst/server/internal/shared/time"
)

// fakeRepo is an in-memory Repository with the same claim/lease semantics as
// the Postgres one: Claim increments attempts and hides the row until it is
// published, retried or poisoned.
type fakeRepo struct {
	mu        sync.Mutex
	rows      []*outbox.Row
	retryAt   map[ids.ID]time.Time
	published map[ids.ID]bool
	poisoned  map[ids.ID]bool
	claimErr  error
}

func newFakeRepo(rows ...outbox.Row) *fakeRepo {
	f := &fakeRepo{retryAt: map[ids.ID]time.Time{}, published: map[ids.ID]bool{}, poisoned: map[ids.ID]bool{}}
	for i := range rows {
		f.rows = append(f.rows, &rows[i])
	}
	return f
}

func (f *fakeRepo) Insert(context.Context, pgx.Tx, outbox.Row) error { return nil }

func (f *fakeRepo) Claim(_ context.Context, limit int, lease time.Duration) ([]outbox.Row, error) {
	f.mu.Lock()
	defer f.mu.Unlock()
	if f.claimErr != nil {
		return nil, f.claimErr
	}
	var out []outbox.Row
	for _, r := range f.rows {
		if len(out) == limit {
			break
		}
		if f.published[r.EventID] || f.poisoned[r.EventID] || time.Now().Before(f.retryAt[r.EventID]) {
			continue
		}
		r.Attempts++
		f.retryAt[r.EventID] = time.Now().Add(lease)
		out = append(out, *r)
	}
	return out, nil
}

func (f *fakeRepo) MarkPublished(_ context.Context, eventIDs []ids.ID) error {
	f.mu.Lock()
	defer f.mu.Unlock()
	for _, id := range eventIDs {
		f.published[id] = true
	}
	return nil
}

func (f *fakeRepo) Retry(_ context.Context, id ids.ID, at time.Time) error {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.retryAt[id] = at
	return nil
}

func (f *fakeRepo) MarkPoison(_ context.Context, id ids.ID) error {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.poisoned[id] = true
	return nil
}

func (f *fakeRepo) Backlog(context.Context) (outbox.Backlog, error) { return outbox.Backlog{}, nil }
func (f *fakeRepo) PurgePublished(context.Context, time.Time, int) (int64, error) {
	return 0, nil
}
func (f *fakeRepo) Get(context.Context, ids.ID) (outbox.Row, error) { return outbox.Row{}, nil }

type publisherFunc func(context.Context, messaging.Event) error

func (p publisherFunc) Publish(ctx context.Context, e messaging.Event) error { return p(ctx, e) }

func newRow() outbox.Row {
	return outbox.NewRow(context.Background(), sharedtime.Fixed{At: time.Now()}, "tasks.v1.TaskCompleted", 1, ids.New(), []byte(`{}`))
}

func newRelay(t *testing.T, repo outbox.Repository, pub messaging.Publisher, cfg outbox.RelayConfig) *outbox.Relay {
	t.Helper()
	cfg.Module = "tasks"
	r, err := outbox.NewRelay(repo, pub, cfg, slog.New(slog.DiscardHandler))
	if err != nil {
		t.Fatalf("NewRelay() error = %v", err)
	}
	return r
}

func TestRelay_DeliversBatchAndMarksPublished(t *testing.T) {
	rows := []outbox.Row{newRow(), newRow(), newRow()}
	repo := newFakeRepo(rows...)
	var (
		gotMu sync.Mutex
		got   []ids.ID
	)
	relay := newRelay(t, repo, publisherFunc(func(_ context.Context, e messaging.Event) error {
		gotMu.Lock()
		got = append(got, e.ID)
		gotMu.Unlock()
		return nil
	}), outbox.RelayConfig{BatchSize: 10})

	n, err := relay.RelayOnce(context.Background())
	if err != nil || n != 3 {
		t.Fatalf("RelayOnce() = %d, %v; want 3, nil", n, err)
	}
	if len(got) != 3 || len(repo.published) != 3 {
		t.Errorf("delivered %d, marked published %d; want 3 and 3", len(got), len(repo.published))
	}
	if n, _ := relay.RelayOnce(context.Background()); n != 0 {
		t.Errorf("second pass claimed %d rows, want 0 (published rows are not redelivered)", n)
	}
}

func TestRelay_RespectsBatchSize(t *testing.T) {
	repo := newFakeRepo(newRow(), newRow(), newRow())
	relay := newRelay(t, repo, publisherFunc(func(context.Context, messaging.Event) error { return nil }), outbox.RelayConfig{BatchSize: 2})
	if n, _ := relay.RelayOnce(context.Background()); n != 2 {
		t.Fatalf("first pass claimed %d, want 2", n)
	}
	if n, _ := relay.RelayOnce(context.Background()); n != 1 {
		t.Fatalf("second pass claimed %d, want 1", n)
	}
}

// One failing event must not block its batch neighbours, and must be
// scheduled for a later retry rather than redelivered immediately.
func TestRelay_FailureIsIsolatedAndBackedOff(t *testing.T) {
	bad, good := newRow(), newRow()
	repo := newFakeRepo(bad, good)
	relay := newRelay(t, repo, publisherFunc(func(_ context.Context, e messaging.Event) error {
		if e.ID == bad.EventID {
			return errors.New("subscriber down")
		}
		return nil
	}), outbox.RelayConfig{BatchSize: 10, MaxBackoff: time.Hour})

	if _, err := relay.RelayOnce(context.Background()); err != nil {
		t.Fatalf("RelayOnce() error = %v, want nil (a failed event is retried, not fatal)", err)
	}
	if !repo.published[good.EventID] || repo.published[bad.EventID] {
		t.Fatalf("published = %v; want only the good event", repo.published)
	}
	if !repo.retryAt[bad.EventID].After(time.Now()) {
		t.Errorf("failed event retryAt = %v, want a future time", repo.retryAt[bad.EventID])
	}
	if n, _ := relay.RelayOnce(context.Background()); n != 0 {
		t.Errorf("failed event was redelivered before its backoff elapsed (%d rows)", n)
	}
}

func TestRelay_PoisonsAfterMaxAttempts(t *testing.T) {
	row := newRow()
	repo := newFakeRepo(row)
	relay := newRelay(t, repo, publisherFunc(func(context.Context, messaging.Event) error { return errors.New("always fails") }),
		outbox.RelayConfig{BatchSize: 10, MaxAttempts: 3, Lease: time.Nanosecond})

	for range 3 {
		repo.mu.Lock()
		repo.retryAt = map[ids.ID]time.Time{} // let the backoff elapse
		repo.mu.Unlock()
		if _, err := relay.RelayOnce(context.Background()); err != nil {
			t.Fatalf("RelayOnce() error = %v", err)
		}
	}
	if !repo.poisoned[row.EventID] {
		t.Fatal("event not poisoned after MaxAttempts failed deliveries")
	}
	repo.mu.Lock()
	repo.retryAt = map[ids.ID]time.Time{}
	repo.mu.Unlock()
	if n, _ := relay.RelayOnce(context.Background()); n != 0 {
		t.Errorf("poisoned event was claimed again (%d rows)", n)
	}
}

func TestRelay_ClaimErrorIsReturned(t *testing.T) {
	repo := newFakeRepo()
	repo.claimErr = errors.New("db down")
	relay := newRelay(t, repo, publisherFunc(func(context.Context, messaging.Event) error { return nil }), outbox.RelayConfig{})
	if _, err := relay.RelayOnce(context.Background()); err == nil {
		t.Fatal("RelayOnce() error = nil, want the claim error")
	}
}

func TestRelay_RunStopsOnContextCancel(t *testing.T) {
	relay := newRelay(t, newFakeRepo(), publisherFunc(func(context.Context, messaging.Event) error { return nil }), outbox.RelayConfig{Interval: time.Millisecond})
	ctx, cancel := context.WithCancel(context.Background())
	done := make(chan error, 1)
	go func() { done <- relay.Run(ctx) }()
	time.Sleep(20 * time.Millisecond)
	cancel()
	select {
	case err := <-done:
		if err != nil {
			t.Errorf("Run() = %v, want nil on shutdown", err)
		}
	case <-time.After(2 * time.Second):
		t.Fatal("Run() did not return after context cancellation")
	}
}

// A batch is published concurrently: with Concurrency 4 and a publisher that
// blocks until four calls are in flight, a serial relay would deadlock.
func TestRelay_PublishesABatchConcurrently(t *testing.T) {
	repo := newFakeRepo(newRow(), newRow(), newRow(), newRow())
	var inFlight atomic.Int32
	gate := make(chan struct{})
	relay := newRelay(t, repo, publisherFunc(func(context.Context, messaging.Event) error {
		if inFlight.Add(1) == 4 {
			close(gate)
		}
		select {
		case <-gate:
			return nil
		case <-time.After(2 * time.Second):
			return errors.New("publishes were not concurrent")
		}
	}), outbox.RelayConfig{BatchSize: 10, Concurrency: 4})

	if _, err := relay.RelayOnce(context.Background()); err != nil {
		t.Fatalf("RelayOnce() error = %v", err)
	}
	if len(repo.published) != 4 {
		t.Errorf("published %d events, want 4 (a serial relay would have timed out)", len(repo.published))
	}
}
