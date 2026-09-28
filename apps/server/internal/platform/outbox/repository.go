package outbox

import (
	"context"
	"time"

	"github.com/jackc/pgx/v5"

	"github.com/aedst/server/internal/shared/ids"
)

// Repository persists outbox rows. Insert runs inside the same transaction
// as the domain write that produced the event; the rest run standalone
// against the module's pool from the Forwarder's polling loop. Implemented
// per module under infrastructure/postgres, backed by that module's own
// schema — outbox rows never cross module boundaries (see docs/database.md).
type Repository interface {
	// Insert writes row inside tx, atomically with the domain change that
	// produced it.
	Insert(ctx context.Context, tx pgx.Tx, row Row) error

	// FetchUnpublishedBatch returns up to limit rows that are neither
	// published nor poisoned, oldest first.
	FetchUnpublishedBatch(ctx context.Context, limit int) ([]Row, error)

	// MarkPublished records that eventIDs were successfully published.
	MarkPublished(ctx context.Context, eventIDs []ids.ID) error

	// IncrementAttempts records a failed publish attempt and schedules the
	// next retry.
	IncrementAttempts(ctx context.Context, eventID ids.ID, nextRetryAt time.Time) error

	// MarkPoison excludes eventID from future fetches after MaxAttempts is
	// exceeded, without deleting the row (poisoned rows stay for inspection).
	MarkPoison(ctx context.Context, eventID ids.ID) error
}
