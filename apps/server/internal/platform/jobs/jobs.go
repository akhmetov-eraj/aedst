// Package jobs wires River for background job processing (email, push
// notifications, media processing, search indexing, cleanup, scheduled
// tasks — see docs/decisions/0005-river-for-jobs.md). Modules register their
// own job kinds/workers here at startup; this package only owns the River
// client lifecycle, not any job's business logic.
package jobs

import (
	"context"
	"fmt"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"
	"github.com/riverqueue/river"
	"github.com/riverqueue/river/riverdriver/riverpgxv5"
)

// Client is the River client type every module's job-enqueuing code depends
// on, parameterized over pgx's transaction type.
type Client = river.Client[pgx.Tx]

// NewClient builds a River client bound to the shared Postgres pool, using
// workers registered by every module (via workers.AddWorker in cmd/server's
// wiring) so a job enqueued in the same transaction as the state change that
// triggers it (see ADR-0005) shares that module's connection pool.
func NewClient(pool *pgxpool.Pool, workers *river.Workers) (*Client, error) {
	client, err := river.NewClient(riverpgxv5.New(pool), &river.Config{
		Queues: map[string]river.QueueConfig{
			river.QueueDefault: {MaxWorkers: 10},
		},
		Workers: workers,
	})
	if err != nil {
		return nil, fmt.Errorf("jobs: new river client: %w", err)
	}
	return client, nil
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
