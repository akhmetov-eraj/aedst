package postgres

import (
	"context"
	"errors"

	"github.com/jackc/pgx/v5"

	tasksdb "github.com/aedst/server/gen/db/tasks"
	sharederrors "github.com/aedst/server/internal/shared/errors"
)

// IdempotencyStore implements idempotency.Store against
// tasks.idempotency_keys. Both methods run inside the command's own
// transaction (see internal/platform/idempotency.Check / Save).
type IdempotencyStore struct{}

func NewIdempotencyStore() *IdempotencyStore {
	return &IdempotencyStore{}
}

func (s *IdempotencyStore) Get(ctx context.Context, tx pgx.Tx, key string) (result []byte, requestHash string, found bool, err error) {
	q := tasksdb.New(tx)
	row, err := q.GetIdempotencyKey(ctx, key)
	if err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return nil, "", false, nil
		}
		return nil, "", false, sharederrors.Wrap("tasks.idempotency.Get", err)
	}
	return row.Result, row.RequestHash, true, nil
}

func (s *IdempotencyStore) Save(ctx context.Context, tx pgx.Tx, key, requestHash string, result []byte) error {
	q := tasksdb.New(tx)
	err := q.SaveIdempotencyKey(ctx, tasksdb.SaveIdempotencyKeyParams{
		Key:         key,
		RequestHash: requestHash,
		Result:      result,
	})
	if err != nil {
		return sharederrors.Wrap("tasks.idempotency.Save", err)
	}
	return nil
}
