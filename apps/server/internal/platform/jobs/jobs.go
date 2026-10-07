// Package jobs wires River for background job processing (email, push
// notifications, media processing, search indexing, cleanup, scheduled
// tasks — see docs/decisions/0005-river-for-jobs.md). Modules register their
// own job kinds/workers here at startup; this package only owns the River
// client lifecycle, not any job's business logic.
package jobs

import (
	"context"
	"fmt"
	"log/slog"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"
	"github.com/riverqueue/river"
	"github.com/riverqueue/river/riverdriver/riverpgxv5"
	"github.com/riverqueue/river/rivermigrate"
)

// Client is the River client type every module's job-enqueuing code depends
// on, parameterized over pgx's transaction type.
type Client = river.Client[pgx.Tx]

// NewClient builds a River client bound to the shared Postgres pool, using
// workers registered by every module (via workers.AddWorker in cmd/server's
// wiring) so a job enqueued in the same transaction as the state change that
// triggers it (see ADR-0005) shares that module's connection pool.
func NewClient(pool *pgxpool.Pool, workers *river.Workers, logger *slog.Logger, periodic ...*river.PeriodicJob) (*Client, error) {
	return NewClientWithQueues(pool, workers, logger, map[string]int{river.QueueDefault: 10}, periodic...)
}

// QueueMedia is the queue of heavy media processing jobs (image variants,
// ffmpeg). It is separate so a dedicated worker deployment can run only it
// (AEDST_JOBS_QUEUES=media) while API replicas run the light queues.
const QueueMedia = "media"

// NewClientWithQueues is NewClient with an explicit set of queues this process
// works (queue name -> max concurrent workers). An empty map makes the client
// insert-only: it can enqueue jobs inside transactions but never runs any,
// which is what an API-only replica wants when workers are deployed apart.
func NewClientWithQueues(pool *pgxpool.Pool, workers *river.Workers, logger *slog.Logger, queues map[string]int, periodic ...*river.PeriodicJob) (*Client, error) {
	cfg := &river.Config{
		Logger:  logger,
		Workers: workers,
	}
	if len(queues) > 0 {
		cfg.Queues = make(map[string]river.QueueConfig, len(queues))
		for name, n := range queues {
			cfg.Queues[name] = river.QueueConfig{MaxWorkers: n}
		}
		cfg.PeriodicJobs = periodic
	}
	client, err := river.NewClient(riverpgxv5.New(pool), cfg)
	if err != nil {
		return nil, fmt.Errorf("jobs: new river client: %w", err)
	}
	return client, nil
}

// Works reports whether client runs jobs (has queues) rather than only
// inserting them.
func Works(queues map[string]int) bool { return len(queues) > 0 }

// PendingCount is the number of jobs waiting to run or to be retried, across
// all queues (operator dashboards, see the admin module).
func PendingCount(ctx context.Context, pool *pgxpool.Pool) (int64, error) {
	var n int64
	err := pool.QueryRow(ctx, `SELECT count(*) FROM river_job WHERE state IN ('available', 'retryable', 'scheduled')`).Scan(&n)
	if err != nil {
		return 0, fmt.Errorf("jobs: pending count: %w", err)
	}
	return n, nil
}

// Migrate applies River's own schema migrations (the river_job/river_leader
// tables etc., see ADR-0005) against the shared Postgres pool. It uses
// River's bundled Go migrator rather than a hand-copied goose SQL file under
// migrations/ so the schema stays byte-for-byte in sync with the installed
// River version; it's idempotent (safe to call on every startup) since the
// migrator tracks applied versions itself in river_migration.
func Migrate(ctx context.Context, pool *pgxpool.Pool) error {
	migrator, err := rivermigrate.New(riverpgxv5.New(pool), nil)
	if err != nil {
		return fmt.Errorf("jobs: new migrator: %w", err)
	}
	if _, err := migrator.Migrate(ctx, rivermigrate.DirectionUp, nil); err != nil {
		return fmt.Errorf("jobs: migrate up: %w", err)
	}
	return nil
}

func Start(ctx context.Context, client *Client) error {
	if err := client.Start(ctx); err != nil {
		return fmt.Errorf("jobs: start: %w", err)
	}
	return nil
}

func Stop(ctx context.Context, client *Client) error {
	if err := client.Stop(ctx); err != nil {
		return fmt.Errorf("jobs: stop: %w", err)
	}
	return nil
}
