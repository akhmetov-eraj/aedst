package outbox_test

import (
	"context"
	"errors"
	"log/slog"
	"testing"
	"time"

	"github.com/jackc/pgx/v5"

	"github.com/aedst/server/internal/platform/outbox"
	"github.com/aedst/server/internal/shared/ids"
	sharedtime "github.com/aedst/server/internal/shared/time"
)

type fakeRepo struct {
	rows        []outbox.Row
	published   []ids.ID
	incremented map[ids.ID]int
	poisoned    map[ids.ID]bool
}

func newFakeRepo(rows ...outbox.Row) *fakeRepo {
	return &fakeRepo{rows: rows, incremented: map[ids.ID]int{}, poisoned: map[ids.ID]bool{}}
}

func (f *fakeRepo) Insert(_ context.Context, _ pgx.Tx, _ outbox.Row) error {
	return nil
}

func (f *fakeRepo) FetchUnpublishedBatch(_ context.Context, limit int) ([]outbox.Row, error) {
	var out []outbox.Row
	for _, r := range f.rows {
		if r.PublishedAt != nil || f.poisoned[r.EventID] {
			continue
		}
		out = append(out, r)
		if len(out) == limit {
			break
		}
	}
	return out, nil
}

func (f *fakeRepo) MarkPublished(_ context.Context, eventIDs []ids.ID) error {
	f.published = append(f.published, eventIDs...)
	for i := range f.rows {
		for _, id := range eventIDs {
			if f.rows[i].EventID == id {
				now := time.Now()
				f.rows[i].PublishedAt = &now
			}
		}
	}
	return nil
}

func (f *fakeRepo) IncrementAttempts(_ context.Context, eventID ids.ID, _ time.Time) error {
	f.incremented[eventID]++
	for i := range f.rows {
		if f.rows[i].EventID == eventID {
			f.rows[i].Attempts++
		}
	}
	return nil
}

func (f *fakeRepo) MarkPoison(_ context.Context, eventID ids.ID) error {
	f.poisoned[eventID] = true
	return nil
}

type fakePublisher struct {
	fail bool
	sent []string
}

func (p *fakePublisher) PublishRaw(_ context.Context, subject, eventID string, _ []byte) error {
	if p.fail {
		return errors.New("publish: broker unavailable")
	}
	p.sent = append(p.sent, subject+"/"+eventID)
	return nil
}

func discardLogger() *slog.Logger {
	return slog.New(slog.DiscardHandler)
}

func TestForwarder_RunOnce_PublishesAndMarksPublished(t *testing.T) {
	clock := sharedtime.Fixed{At: time.Now()}
	row := outbox.NewRow(clock, "tasks.v1.TaskCompleted", 1, ids.New(), "", []byte(`{"ok":true}`))
	repo := newFakeRepo(row)
	pub := &fakePublisher{}

	fwd := outbox.NewForwarder("tasks", repo, pub, outbox.JSONEnvelope, discardLogger(), clock, outbox.DefaultForwarderConfig())

	if err := fwd.RunOnce(context.Background()); err != nil {
		t.Fatalf("RunOnce: %v", err)
	}

	if len(pub.sent) != 1 {
		t.Fatalf("expected 1 published message, got %d", len(pub.sent))
	}
	if len(repo.published) != 1 || repo.published[0] != row.EventID {
		t.Fatalf("expected row %s marked published, got %v", row.EventID, repo.published)
	}
}

func TestForwarder_RunOnce_RetriesOnPublishFailure(t *testing.T) {
	clock := sharedtime.Fixed{At: time.Now()}
	row := outbox.NewRow(clock, "tasks.v1.TaskCompleted", 1, ids.New(), "", []byte(`{}`))
	repo := newFakeRepo(row)
	pub := &fakePublisher{fail: true}

	cfg := outbox.DefaultForwarderConfig()
	cfg.MaxAttempts = 3
	fwd := outbox.NewForwarder("tasks", repo, pub, outbox.JSONEnvelope, discardLogger(), clock, cfg)

	if err := fwd.RunOnce(context.Background()); err != nil {
		t.Fatalf("RunOnce: %v", err)
	}

	if repo.incremented[row.EventID] != 1 {
		t.Fatalf("expected 1 attempt recorded, got %d", repo.incremented[row.EventID])
	}
	if len(repo.published) != 0 {
		t.Fatalf("expected no rows published on failure, got %v", repo.published)
	}
	if repo.poisoned[row.EventID] {
		t.Fatalf("row should not be poisoned before MaxAttempts is reached")
	}
}

func TestForwarder_RunOnce_PoisonsAfterMaxAttempts(t *testing.T) {
	clock := sharedtime.Fixed{At: time.Now()}
	row := outbox.NewRow(clock, "tasks.v1.TaskCompleted", 1, ids.New(), "", []byte(`{}`))
	row.Attempts = 2 // one more failure reaches MaxAttempts=3
	repo := newFakeRepo(row)
	pub := &fakePublisher{fail: true}

	cfg := outbox.DefaultForwarderConfig()
	cfg.MaxAttempts = 3
	fwd := outbox.NewForwarder("tasks", repo, pub, outbox.JSONEnvelope, discardLogger(), clock, cfg)

	if err := fwd.RunOnce(context.Background()); err != nil {
		t.Fatalf("RunOnce: %v", err)
	}

	if !repo.poisoned[row.EventID] {
		t.Fatalf("expected row to be poisoned after reaching MaxAttempts")
	}
}

func TestForwarder_RunOnce_NoRows_NoOp(t *testing.T) {
	repo := newFakeRepo()
	pub := &fakePublisher{}
	clock := sharedtime.Fixed{At: time.Now()}

	fwd := outbox.NewForwarder("tasks", repo, pub, outbox.JSONEnvelope, discardLogger(), clock, outbox.DefaultForwarderConfig())
	if err := fwd.RunOnce(context.Background()); err != nil {
		t.Fatalf("RunOnce: %v", err)
	}
	if len(pub.sent) != 0 {
		t.Fatalf("expected nothing published, got %v", pub.sent)
	}
}
