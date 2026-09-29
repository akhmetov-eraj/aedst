package postgres

import (
	"context"
	"time"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgtype"
	"github.com/jackc/pgx/v5/pgxpool"

	tasksdb "github.com/aedst/server/gen/db/tasks"
	"github.com/aedst/server/internal/platform/outbox"
	sharederrors "github.com/aedst/server/internal/shared/errors"
	"github.com/aedst/server/internal/shared/ids"
)

// OutboxRepository implements outbox.Repository against tasks.outbox.
// Insert runs inside the caller's transaction; the rest run standalone
// against the pool, driven by the Forwarder's polling loop.
type OutboxRepository struct {
	pool *pgxpool.Pool
}

func NewOutboxRepository(pool *pgxpool.Pool) *OutboxRepository {
	return &OutboxRepository{pool: pool}
}

func (r *OutboxRepository) Insert(ctx context.Context, tx pgx.Tx, row outbox.Row) error {
	q := tasksdb.New(tx)
	err := q.InsertOutboxRow(ctx, tasksdb.InsertOutboxRowParams{
		EventID:      toPgUUID(row.EventID),
		EventType:    row.EventType,
		EventVersion: int32(row.EventVersion),
		OccurredAt:   toPgTimestamptz(row.OccurredAt),
		PartitionKey: toPgUUID(row.PartitionKey),
		Traceparent:  row.Traceparent,
		Payload:      row.Payload,
	})
	if err != nil {
		return sharederrors.Wrap("tasks.outbox.Insert", err)
	}
	return nil
}

func (r *OutboxRepository) FetchUnpublishedBatch(ctx context.Context, limit int) ([]outbox.Row, error) {
	q := tasksdb.New(r.pool)
	rows, err := q.FetchUnpublishedOutboxRows(ctx, int32(limit))
	if err != nil {
		return nil, sharederrors.Wrap("tasks.outbox.FetchUnpublishedBatch", err)
	}

	out := make([]outbox.Row, len(rows))
	for i, row := range rows {
		out[i] = outbox.Row{
			EventID:      fromPgUUID(row.EventID),
			EventType:    row.EventType,
			EventVersion: uint32(row.EventVersion),
			OccurredAt:   row.OccurredAt.Time,
			PartitionKey: fromPgUUID(row.PartitionKey),
			Traceparent:  row.Traceparent,
			Payload:      row.Payload,
			PublishedAt:  fromPgTimestamptzPtr(row.PublishedAt),
			Attempts:     int(row.Attempts),
			NextRetryAt:  fromPgTimestamptzPtr(row.NextRetryAt),
		}
	}
	return out, nil
}

func (r *OutboxRepository) MarkPublished(ctx context.Context, eventIDs []ids.ID) error {
	q := tasksdb.New(r.pool)
	pgIDs := make([]pgtype.UUID, len(eventIDs))
	for i, id := range eventIDs {
		pgIDs[i] = toPgUUID(id)
	}
	if err := q.MarkOutboxRowsPublished(ctx, pgIDs); err != nil {
		return sharederrors.Wrap("tasks.outbox.MarkPublished", err)
	}
	return nil
}

func (r *OutboxRepository) IncrementAttempts(ctx context.Context, eventID ids.ID, nextRetryAt time.Time) error {
	q := tasksdb.New(r.pool)
	err := q.IncrementOutboxAttempts(ctx, tasksdb.IncrementOutboxAttemptsParams{
		EventID:     toPgUUID(eventID),
		NextRetryAt: toPgTimestamptz(nextRetryAt),
	})
	if err != nil {
		return sharederrors.Wrap("tasks.outbox.IncrementAttempts", err)
	}
	return nil
}

func (r *OutboxRepository) MarkPoison(ctx context.Context, eventID ids.ID) error {
	q := tasksdb.New(r.pool)
	if err := q.MarkOutboxRowPoisoned(ctx, toPgUUID(eventID)); err != nil {
		return sharederrors.Wrap("tasks.outbox.MarkPoison", err)
	}
	return nil
}
