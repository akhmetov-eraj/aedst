// Package inbox implements the consumer-side counterpart to
// internal/platform/outbox: it turns the at-least-once delivery of the relay
// (and a crashed-mid-processing subscriber) into effectively-once processing,
// by checking and recording "already handled" atomically with the handler's
// own database side effect, in one transaction. The handler is still expected
// to be idempotent on its own as defense in depth — this package makes that
// defense unnecessary in the common case, not optional in the rare one.
//
// Dedup is keyed by (consumer name, event id) in the shared platform.inbox
// table, so several consumers of one event dedupe independently and a module
// needs no inbox table or repository of its own.
package inbox

import (
	"context"
	"fmt"
	"time"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"

	"github.com/aedst/server/internal/shared/ids"
)

// Repository tracks which event ids a consumer has already processed. Both
// methods run inside the consumer's transaction.
type Repository interface {
	IsProcessed(ctx context.Context, tx pgx.Tx, eventID ids.ID) (bool, error)
	MarkProcessed(ctx context.Context, tx pgx.Tx, eventID ids.ID) error
}

// PostgresRepository is the platform.inbox implementation of Repository for
// one named consumer.
type PostgresRepository struct{ consumer string }

var _ Repository = PostgresRepository{}

// NewPostgresRepository returns the repository for consumer, e.g.
// "notifications.task_completed".
func NewPostgresRepository(consumer string) PostgresRepository {
	return PostgresRepository{consumer: consumer}
}

func (r PostgresRepository) IsProcessed(ctx context.Context, tx pgx.Tx, eventID ids.ID) (bool, error) {
	var processed bool
	err := tx.QueryRow(ctx,
		`SELECT EXISTS (SELECT 1 FROM platform.inbox WHERE consumer = $1 AND event_id = $2)`,
		r.consumer, eventID.String()).Scan(&processed)
	if err != nil {
		return false, fmt.Errorf("inbox: is processed: %w", err)
	}
	return processed, nil
}

func (r PostgresRepository) MarkProcessed(ctx context.Context, tx pgx.Tx, eventID ids.ID) error {
	_, err := tx.Exec(ctx,
		`INSERT INTO platform.inbox (consumer, event_id) VALUES ($1, $2) ON CONFLICT DO NOTHING`,
		r.consumer, eventID.String())
	if err != nil {
		return fmt.Errorf("inbox: mark processed: %w", err)
	}
	return nil
}

// Purge deletes up to limit inbox rows processed before cutoff and returns how
// many it deleted. Rows only need to outlive the outbox's redelivery window
// (retention of published events plus the longest retry backoff).
func Purge(ctx context.Context, pool *pgxpool.Pool, cutoff time.Time, limit int) (int64, error) {
	tag, err := pool.Exec(ctx, `
DELETE FROM platform.inbox
WHERE (consumer, event_id) IN (
    SELECT consumer, event_id FROM platform.inbox WHERE processed_at < $1 LIMIT $2
)`, cutoff, int32(limit))
	if err != nil {
		return 0, fmt.Errorf("inbox: purge: %w", err)
	}
	return tag.RowsAffected(), nil
}
