package postgres

import (
	"context"

	"github.com/jackc/pgx/v5"

	tasksdb "github.com/aedst/server/gen/db/tasks"
	sharederrors "github.com/aedst/server/internal/shared/errors"
	"github.com/aedst/server/internal/shared/ids"
)

// InboxRepository implements inbox.Repository against tasks.inbox. Both
// methods run inside the consumer's transaction (see
// internal/platform/inbox.Consumer).
type InboxRepository struct{}

func NewInboxRepository() *InboxRepository {
	return &InboxRepository{}
}

func (r *InboxRepository) IsProcessed(ctx context.Context, tx pgx.Tx, eventID ids.ID) (bool, error) {
	q := tasksdb.New(tx)
	processed, err := q.IsInboxEventProcessed(ctx, toPgUUID(eventID))
	if err != nil {
		return false, sharederrors.Wrap("tasks.inbox.IsProcessed", err)
	}
	return processed, nil
}

func (r *InboxRepository) MarkProcessed(ctx context.Context, tx pgx.Tx, eventID ids.ID) error {
	q := tasksdb.New(tx)
	if err := q.MarkInboxEventProcessed(ctx, toPgUUID(eventID)); err != nil {
		return sharederrors.Wrap("tasks.inbox.MarkProcessed", err)
	}
	return nil
}
