package idempotency

import (
	"context"
	"errors"
	"fmt"
	"time"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"

	"github.com/aedst/server/internal/platform/lock"
)

// PostgresStore implements Store on the shared platform.idempotency_keys
// table. It is stateless: every method runs inside the command's own
// transaction.
type PostgresStore struct{}

var _ Store = PostgresStore{}

func NewPostgresStore() PostgresStore { return PostgresStore{} }

func (PostgresStore) Acquire(ctx context.Context, tx pgx.Tx, scope, key string) error {
	if err := lock.Xact(ctx, tx, "idempotency:"+scope+":"+key); err != nil {
		return fmt.Errorf("lock key: %w", err)
	}
	return nil
}

func (PostgresStore) Get(ctx context.Context, tx pgx.Tx, scope, key string) (result []byte, requestHash string, found bool, err error) {
	err = tx.QueryRow(ctx,
		`SELECT result, request_hash FROM platform.idempotency_keys WHERE scope = $1 AND key = $2`,
		scope, key).Scan(&result, &requestHash)
	if errors.Is(err, pgx.ErrNoRows) {
		return nil, "", false, nil
	}
	if err != nil {
		return nil, "", false, fmt.Errorf("select: %w", err)
	}
	return result, requestHash, true, nil
}

func (PostgresStore) Save(ctx context.Context, tx pgx.Tx, scope, key, requestHash string, result []byte) error {
	// Acquire serialized concurrent writers, so a conflict here can only mean
	// a caller skipped Check; keep the first result then.
	_, err := tx.Exec(ctx, `
INSERT INTO platform.idempotency_keys (scope, key, request_hash, result)
VALUES ($1, $2, $3, $4)
ON CONFLICT (scope, key) DO NOTHING`, scope, key, requestHash, result)
	if err != nil {
		return fmt.Errorf("insert: %w", err)
	}
	return nil
}

// Purge deletes up to limit keys created before cutoff and returns how many
// it deleted. A key only needs to outlive a client's retry window.
func Purge(ctx context.Context, pool *pgxpool.Pool, cutoff time.Time, limit int) (int64, error) {
	tag, err := pool.Exec(ctx, `
DELETE FROM platform.idempotency_keys
WHERE (scope, key) IN (
    SELECT scope, key FROM platform.idempotency_keys WHERE created_at < $1 LIMIT $2
)`, cutoff, int32(limit))
	if err != nil {
		return 0, fmt.Errorf("idempotency: purge: %w", err)
	}
	return tag.RowsAffected(), nil
}
