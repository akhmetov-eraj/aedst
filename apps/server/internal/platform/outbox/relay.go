package outbox

import (
	"context"
	"errors"
	"fmt"
	"log/slog"
	"math"
	"sync"
	"time"

	"golang.org/x/sync/errgroup"

	"github.com/aedst/server/internal/platform/messaging"
	"github.com/aedst/server/internal/shared/ids"
)

// RelayConfig tunes one module's Relay. Zero values take the defaults below.
type RelayConfig struct {
	// Module names the module whose outbox this relay drains (logs, metrics).
	Module string
	// Interval is how often an idle relay polls for new rows.
	Interval time.Duration
	// BatchSize is the most rows claimed per pass. A pass that fills the batch
	// immediately runs again, so a backlog drains at full speed.
	BatchSize int
	// Concurrency is how many rows of a batch are published in parallel.
	Concurrency int
	// MaxAttempts is how many deliveries a row gets before it is poisoned.
	MaxAttempts int
	// Lease is how long a claimed row stays invisible to other relays. It must
	// exceed the time to publish one batch; on expiry a crashed relay's rows
	// are claimed again.
	Lease time.Duration
	// MaxBackoff caps the exponential retry delay.
	MaxBackoff time.Duration
	// Retention is how long published rows are kept before being purged.
	Retention time.Duration
}

const (
	defaultInterval    = 500 * time.Millisecond
	defaultBatchSize   = 100
	defaultConcurrency = 4
	defaultMaxAttempts = 10
	defaultLease       = time.Minute
	defaultMaxBackoff  = 5 * time.Minute
	defaultRetention   = 7 * 24 * time.Hour

	backlogRefreshEvery = 5 * time.Second
	purgeEvery          = 10 * time.Minute
	purgeBatch          = 5_000
)

func (c *RelayConfig) applyDefaults() {
	if c.Interval <= 0 {
		c.Interval = defaultInterval
	}
	if c.BatchSize <= 0 {
		c.BatchSize = defaultBatchSize
	}
	if c.Concurrency <= 0 {
		c.Concurrency = defaultConcurrency
	}
	if c.MaxAttempts <= 0 {
		c.MaxAttempts = defaultMaxAttempts
	}
	if c.Lease <= 0 {
		c.Lease = defaultLease
	}
	if c.MaxBackoff <= 0 {
		c.MaxBackoff = defaultMaxBackoff
	}
	if c.Retention <= 0 {
		c.Retention = defaultRetention
	}
}

// Relay delivers a module's outbox rows to a messaging.Publisher in batches.
// Run one per module per process; any number of processes may run the same
// relay concurrently (rows are leased, not shared).
type Relay struct {
	cfg       RelayConfig
	repo      Repository
	publisher messaging.Publisher
	logger    *slog.Logger
	metrics   *metrics
	now       func() time.Time
}

// NewRelay builds a relay for cfg.Module, reading rows through repo.
func NewRelay(repo Repository, publisher messaging.Publisher, cfg RelayConfig, logger *slog.Logger) (*Relay, error) {
	cfg.applyDefaults()
	m, err := newMetrics(cfg.Module)
	if err != nil {
		return nil, fmt.Errorf("outbox: metrics: %w", err)
	}
	return &Relay{
		cfg:       cfg,
		repo:      repo,
		publisher: publisher,
		logger:    logger.With(slog.String("module", cfg.Module)),
		metrics:   m,
		now:       time.Now,
	}, nil
}

// Run polls until ctx is cancelled. It never returns an error for a transient
// failure — a database blip must not kill the process, and unpublished rows
// stay in the table for the next pass — it only logs.
func (r *Relay) Run(ctx context.Context) error {
	ticker := time.NewTicker(r.cfg.Interval)
	defer ticker.Stop()

	var lastBacklog, lastPurge time.Time
	// maintain refreshes the lag gauges and purges old rows on their own
	// cadence. It is called after every pass — including the passes of a long
	// drain — because a relay that is permanently behind never idles, and the
	// gauges must be freshest exactly then.
	maintain := func() {
		if now := r.now(); now.Sub(lastBacklog) >= backlogRefreshEvery {
			lastBacklog = now
			if b, err := r.repo.Backlog(ctx); err != nil {
				r.logger.WarnContext(ctx, "outbox: read backlog", slog.Any("error", err))
			} else {
				r.metrics.recordBacklog(b)
			}
		}
		if now := r.now(); now.Sub(lastPurge) >= purgeEvery {
			lastPurge = now
			r.purge(ctx)
		}
	}

	for {
		// Drain: keep going while passes fill the batch.
		for {
			n, err := r.RelayOnce(ctx)
			maintain()
			if err != nil {
				if ctx.Err() == nil {
					r.logger.ErrorContext(ctx, "outbox: relay pass failed", slog.Any("error", err))
				}
				break
			}
			if n < r.cfg.BatchSize || ctx.Err() != nil {
				break
			}
		}

		select {
		case <-ctx.Done():
			return nil
		case <-ticker.C:
		}
	}
}

// RelayOnce claims and delivers one batch and returns how many rows it
// claimed. Exported for tests and for tooling that wants to force a drain.
func (r *Relay) RelayOnce(ctx context.Context) (int, error) {
	rows, err := r.repo.Claim(ctx, r.cfg.BatchSize, r.cfg.Lease)
	if err != nil {
		return 0, fmt.Errorf("outbox: claim: %w", err)
	}
	if len(rows) == 0 {
		return 0, nil
	}
	r.metrics.batchSize.Record(ctx, int64(len(rows)), r.metrics.attrs)

	// Bookkeeping must survive a shutdown that cancelled ctx mid-batch:
	// events already handed to subscribers are recorded as delivered instead
	// of being redelivered on the next start.
	bookCtx, cancel := context.WithTimeout(context.WithoutCancel(ctx), 10*time.Second)
	defer cancel()

	// Rows are published concurrently: each subscriber does its own database
	// transaction, so a serial loop makes one relay's throughput
	// 1/(subscriber latency). Order was never guaranteed (see events.md), so
	// nothing is lost by it.
	var (
		mu        sync.Mutex
		delivered = make([]ids.ID, 0, len(rows))
		errs      []error
		group     errgroup.Group
	)
	group.SetLimit(r.cfg.Concurrency)
	for _, row := range rows {
		group.Go(func() error {
			err := r.publisher.Publish(contextWithTraceparent(ctx, row.Traceparent), messaging.Event{
				ID:           row.EventID,
				Type:         row.EventType,
				Version:      row.EventVersion,
				OccurredAt:   row.OccurredAt,
				PartitionKey: row.PartitionKey,
				Payload:      row.Payload,
			})
			if err == nil {
				mu.Lock()
				delivered = append(delivered, row.EventID)
				mu.Unlock()
				return nil
			}
			if failure := r.handleFailure(bookCtx, row, err); failure != nil {
				mu.Lock()
				errs = append(errs, failure)
				mu.Unlock()
			}
			return nil
		})
	}
	_ = group.Wait() // workers never return an error; failures are collected above

	if len(delivered) > 0 {
		// If this fails the rows stay leased and are delivered again once the
		// lease expires — at-least-once, deduplicated by consumers' inboxes.
		if err := r.repo.MarkPublished(bookCtx, delivered); err != nil {
			errs = append(errs, fmt.Errorf("outbox: mark published: %w", err))
		} else {
			r.metrics.delivered.Add(bookCtx, int64(len(delivered)), r.metrics.attrs)
		}
	}
	return len(rows), errors.Join(errs...)
}

// handleFailure retries row with exponential backoff, or poisons it once its
// attempts (already incremented by Claim) are exhausted.
func (r *Relay) handleFailure(ctx context.Context, row Row, cause error) error {
	r.metrics.failed.Add(ctx, 1, r.metrics.attrs)
	log := r.logger.With(
		slog.String("event_id", row.EventID.String()),
		slog.String("event_type", row.EventType),
		slog.Int("attempt", row.Attempts),
	)

	if row.Attempts >= r.cfg.MaxAttempts {
		log.ErrorContext(ctx, "outbox: event poisoned after exhausting attempts", slog.Any("error", cause))
		r.metrics.poisoned.Add(ctx, 1, r.metrics.attrs)
		if err := r.repo.MarkPoison(ctx, row.EventID); err != nil {
			return fmt.Errorf("outbox: mark poison %s: %w", row.EventID, err)
		}
		return nil
	}

	log.WarnContext(ctx, "outbox: delivery failed, will retry", slog.Any("error", cause))
	if err := r.repo.Retry(ctx, row.EventID, r.now().Add(backoff(row.Attempts, r.cfg.MaxBackoff))); err != nil {
		return fmt.Errorf("outbox: schedule retry %s: %w", row.EventID, err)
	}
	return nil
}

func (r *Relay) purge(ctx context.Context) {
	cutoff := r.now().Add(-r.cfg.Retention)
	for {
		n, err := r.repo.PurgePublished(ctx, cutoff, purgeBatch)
		if err != nil {
			r.logger.WarnContext(ctx, "outbox: purge published rows", slog.Any("error", err))
			return
		}
		if n < purgeBatch {
			return
		}
	}
}

// backoff is 1s, 2s, 4s, ... capped at max, for the given (1-based) attempt.
func backoff(attempt int, max time.Duration) time.Duration {
	d := time.Duration(math.Pow(2, float64(attempt-1))) * time.Second
	if d <= 0 || d > max {
		return max
	}
	return d
}
