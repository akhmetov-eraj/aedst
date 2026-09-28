package outbox

import (
	"context"
	"crypto/rand"
	"fmt"
	"log/slog"
	"math/big"
	"time"

	sharedids "github.com/aedst/server/internal/shared/ids"
	sharedtime "github.com/aedst/server/internal/shared/time"
)

// Publisher publishes a pre-serialized event to subject, tagged with
// eventID so the receiving side's inbox can dedupe on it. Implemented by
// *messaging.Client's PublishRaw (see internal/platform/messaging) — kept as
// its own minimal interface here so this package doesn't depend on NATS.
type Publisher interface {
	PublishRaw(ctx context.Context, subject, eventID string, payload []byte) error
}

// ForwarderConfig tunes the polling loop.
type ForwarderConfig struct {
	BatchSize    int
	PollInterval time.Duration
	MaxAttempts  int
	BackoffBase  time.Duration
	BackoffMax   time.Duration
}

// DefaultForwarderConfig mirrors sane production defaults: poll every
// second, up to 100 rows per batch, retry with jittered exponential backoff
// up to 10 times before poisoning a row.
func DefaultForwarderConfig() ForwarderConfig {
	return ForwarderConfig{
		BatchSize:    100,
		PollInterval: time.Second,
		MaxAttempts:  10,
		BackoffBase:  100 * time.Millisecond,
		BackoffMax:   30 * time.Second,
	}
}

// Forwarder polls Repository for unpublished rows and publishes them via
// Publisher. Run as a background goroutine from cmd/server, one per module
// that owns outbox rows.
type Forwarder struct {
	module    string
	repo      Repository
	publisher Publisher
	marshal   EnvelopeMarshaler
	logger    *slog.Logger
	clock     sharedtime.Clock
	cfg       ForwarderConfig
}

func NewForwarder(module string, repo Repository, publisher Publisher, marshal EnvelopeMarshaler, logger *slog.Logger, clock sharedtime.Clock, cfg ForwarderConfig) *Forwarder {
	return &Forwarder{
		module:    module,
		repo:      repo,
		publisher: publisher,
		marshal:   marshal,
		logger:    logger,
		clock:     clock,
		cfg:       cfg,
	}
}

// Run polls until ctx is cancelled. Errors from individual RunOnce calls are
// logged, not returned — a transient DB or broker hiccup should not stop the
// loop from retrying on the next tick.
func (f *Forwarder) Run(ctx context.Context) error {
	ticker := time.NewTicker(f.cfg.PollInterval)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			return nil
		case <-ticker.C:
			if err := f.RunOnce(ctx); err != nil {
				f.logger.ErrorContext(ctx, "outbox: poll failed", slog.String("module", f.module), slog.Any("error", err))
			}
		}
	}
}

// RunOnce fetches and publishes a single batch. Exported so it can be driven
// directly in tests without waiting on the ticker.
func (f *Forwarder) RunOnce(ctx context.Context) error {
	rows, err := f.repo.FetchUnpublishedBatch(ctx, f.cfg.BatchSize)
	if err != nil {
		return fmt.Errorf("outbox: fetch batch: %w", err)
	}
	if len(rows) == 0 {
		return nil
	}

	published := make([]sharedids.ID, 0, len(rows))
	for _, r := range rows {
		if err := f.forwardOne(ctx, r); err != nil {
			f.logger.WarnContext(ctx, "outbox: forward failed",
				slog.String("module", f.module),
				slog.String("event_id", r.EventID.String()),
				slog.String("event_type", r.EventType),
				slog.Any("error", err),
			)
			continue
		}
		published = append(published, r.EventID)
	}
	if len(published) == 0 {
		return nil
	}

	if err := f.repo.MarkPublished(ctx, published); err != nil {
		return fmt.Errorf("outbox: mark published: %w", err)
	}
	return nil
}

func (f *Forwarder) forwardOne(ctx context.Context, r Row) error {
	payload, err := f.marshal(r)
	if err != nil {
		return fmt.Errorf("marshal envelope: %w", err)
	}

	if err := f.publisher.PublishRaw(ctx, r.EventType, r.EventID.String(), payload); err != nil {
		nextRetryAt := f.clock.Now().Add(backoffWithJitter(f.cfg.BackoffBase, f.cfg.BackoffMax, r.Attempts))
		if markErr := f.repo.IncrementAttempts(ctx, r.EventID, nextRetryAt); markErr != nil {
			f.logger.ErrorContext(ctx, "outbox: increment attempts failed", slog.Any("error", markErr))
		}
		if r.Attempts+1 >= f.cfg.MaxAttempts {
			if poisonErr := f.repo.MarkPoison(ctx, r.EventID); poisonErr != nil {
				f.logger.ErrorContext(ctx, "outbox: mark poison failed", slog.Any("error", poisonErr))
			}
		}
		return fmt.Errorf("publish: %w", err)
	}
	return nil
}

// backoffWithJitter returns base*2^attempt capped at max, with full jitter
// (a uniform random value between 0 and the capped backoff) to avoid
// thundering-herd retries across Forwarder replicas.
func backoffWithJitter(base, max time.Duration, attempt int) time.Duration {
	backoff := base
	for range attempt {
		backoff *= 2
		if backoff >= max {
			backoff = max
			break
		}
	}
	n, err := rand.Int(rand.Reader, big.NewInt(int64(backoff)+1))
	if err != nil {
		return backoff
	}
	return time.Duration(n.Int64())
}
