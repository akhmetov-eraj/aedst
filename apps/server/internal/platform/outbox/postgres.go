package outbox

import (
	"context"
	"errors"
	"fmt"
	"slices"
	"time"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"

	sharederrors "github.com/aedst/server/internal/shared/errors"
	"github.com/aedst/server/internal/shared/ids"
)

// PostgresRepository implements Repository on the shared platform.outbox
// table. One instance serves one module: every statement is scoped by the
// module column, so modules never see (or claim) each other's rows, and a
// module extracted into its own service takes its rows with it by
// `WHERE module = ...`. Insert runs inside the caller's transaction; the rest
// run standalone against pool, driven by Relay.
type PostgresRepository struct {
	pool   *pgxpool.Pool
	module string
}

var _ Repository = (*PostgresRepository)(nil)

// NewPostgresRepository builds the repository of module (e.g. "tasks") over
// pool, which must be the pool the Relay should use (the jobs pool).
func NewPostgresRepository(pool *pgxpool.Pool, module string) *PostgresRepository {
	return &PostgresRepository{pool: pool, module: module}
}

const rowColumns = `event_id, event_type, event_version, occurred_at, partition_key, traceparent, payload, published_at, attempts, poisoned_at`

func (r *PostgresRepository) Insert(ctx context.Context, tx pgx.Tx, row Row) error {
	_, err := tx.Exec(ctx, `
INSERT INTO platform.outbox (event_id, module, event_type, event_version, occurred_at, partition_key, traceparent, payload)
VALUES ($1, $2, $3, $4, $5, $6, $7, $8)`,
		row.EventID.String(), r.module, row.EventType, int32(row.EventVersion), row.OccurredAt, row.PartitionKey.String(), row.Traceparent, row.Payload)
	if err != nil {
		return sharederrors.Wrap(r.module+".outbox.Insert", err)
	}
	return nil
}

func (r *PostgresRepository) Get(ctx context.Context, eventID ids.ID) (Row, error) {
	row, err := scanRow(r.pool.QueryRow(ctx,
		`SELECT `+rowColumns+` FROM platform.outbox WHERE module = $1 AND event_id = $2`, r.module, eventID.String()))
	if err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return Row{}, sharederrors.NotFound(r.module+".outbox.Get", "outbox row not found")
		}
		return Row{}, sharederrors.Wrap(r.module+".outbox.Get", err)
	}
	return row, nil
}

// Claim leases a batch. The inner SELECT ... FOR UPDATE SKIP LOCKED makes
// concurrent relays (other replicas) take disjoint rows; the outer UPDATE
// counts the attempt up front (so an event that crashes the process still
// counts) and hides the rows until next_retry_at. Served by
// idx_platform_outbox_unpublished.
func (r *PostgresRepository) Claim(ctx context.Context, limit int, lease time.Duration) ([]Row, error) {
	rows, err := r.pool.Query(ctx, `
UPDATE platform.outbox
SET attempts = attempts + 1,
    next_retry_at = now() + make_interval(secs => $3::float8)
WHERE event_id IN (
    SELECT o.event_id
    FROM platform.outbox o
    WHERE o.module = $1
      AND o.published_at IS NULL
      AND o.poisoned_at IS NULL
      AND (o.next_retry_at IS NULL OR o.next_retry_at <= now())
    ORDER BY o.occurred_at, o.event_id
    LIMIT $2
    FOR UPDATE SKIP LOCKED
)
RETURNING `+rowColumns, r.module, int32(limit), lease.Seconds())
	if err != nil {
		return nil, sharederrors.Wrap(r.module+".outbox.Claim", err)
	}
	out, err := pgx.CollectRows(rows, func(row pgx.CollectableRow) (Row, error) { return scanRow(row) })
	if err != nil {
		return nil, sharederrors.Wrap(r.module+".outbox.Claim", err)
	}
	// RETURNING has no defined order; deliver oldest first.
	slices.SortFunc(out, func(a, b Row) int { return a.OccurredAt.Compare(b.OccurredAt) })
	return out, nil
}

func (r *PostgresRepository) MarkPublished(ctx context.Context, eventIDs []ids.ID) error {
	_, err := r.pool.Exec(ctx, `
UPDATE platform.outbox SET published_at = now(), next_retry_at = NULL
WHERE module = $1 AND event_id = ANY ($2::uuid[])`, r.module, idStrings(eventIDs))
	if err != nil {
		return sharederrors.Wrap(r.module+".outbox.MarkPublished", err)
	}
	return nil
}

func (r *PostgresRepository) Retry(ctx context.Context, eventID ids.ID, retryAt time.Time) error {
	_, err := r.pool.Exec(ctx,
		`UPDATE platform.outbox SET next_retry_at = $3 WHERE module = $1 AND event_id = $2`, r.module, eventID.String(), retryAt)
	if err != nil {
		return sharederrors.Wrap(r.module+".outbox.Retry", err)
	}
	return nil
}

func (r *PostgresRepository) MarkPoison(ctx context.Context, eventID ids.ID) error {
	_, err := r.pool.Exec(ctx,
		`UPDATE platform.outbox SET poisoned_at = now(), next_retry_at = NULL WHERE module = $1 AND event_id = $2`, r.module, eventID.String())
	if err != nil {
		return sharederrors.Wrap(r.module+".outbox.MarkPoison", err)
	}
	return nil
}

// Backlog's pending count is capped so measuring a huge backlog is not itself
// a huge query; the oldest pending row is one index probe.
func (r *PostgresRepository) Backlog(ctx context.Context) (Backlog, error) {
	var (
		pending int64
		oldest  *time.Time
	)
	err := r.pool.QueryRow(ctx, `
SELECT
    (SELECT count(*) FROM (
        SELECT 1 FROM platform.outbox
        WHERE module = $1 AND published_at IS NULL AND poisoned_at IS NULL
        LIMIT $2
    ) capped)::bigint,
    (SELECT min(occurred_at) FROM platform.outbox
     WHERE module = $1 AND published_at IS NULL AND poisoned_at IS NULL)`,
		r.module, int32(BacklogCap)).Scan(&pending, &oldest)
	if err != nil {
		return Backlog{}, sharederrors.Wrap(r.module+".outbox.Backlog", err)
	}
	b := Backlog{Pending: pending}
	if oldest != nil {
		b.Oldest = *oldest
	}
	return b, nil
}

func (r *PostgresRepository) PurgePublished(ctx context.Context, cutoff time.Time, limit int) (int64, error) {
	tag, err := r.pool.Exec(ctx, `
DELETE FROM platform.outbox
WHERE event_id IN (
    SELECT o.event_id FROM platform.outbox o
    WHERE o.module = $1 AND o.published_at IS NOT NULL AND o.published_at < $2
    LIMIT $3
)`, r.module, cutoff, int32(limit))
	if err != nil {
		return 0, sharederrors.Wrap(r.module+".outbox.PurgePublished", err)
	}
	return tag.RowsAffected(), nil
}

func idStrings(in []ids.ID) []string {
	out := make([]string, len(in))
	for i, id := range in {
		out[i] = id.String()
	}
	return out
}

func scanRow(row pgx.Row) (Row, error) {
	var (
		out     Row
		version int32
		attempt int32
		eventID string
		partKey string
	)
	if err := row.Scan(&eventID, &out.EventType, &version, &out.OccurredAt, &partKey,
		&out.Traceparent, &out.Payload, &out.PublishedAt, &attempt, &out.PoisonedAt); err != nil {
		return Row{}, err
	}
	var err error
	if out.EventID, err = ids.Parse(eventID); err != nil {
		return Row{}, err
	}
	if out.PartitionKey, err = ids.Parse(partKey); err != nil {
		return Row{}, err
	}
	out.EventVersion = uint32(version)
	out.Attempts = int(attempt)
	return out, nil
}

// OldestPendingAge is the age of the oldest undelivered (not yet published,
// not poisoned) event across every module — the outbox lag an operator
// dashboard shows. Zero when nothing is pending.
func OldestPendingAge(ctx context.Context, pool *pgxpool.Pool, now time.Time) (time.Duration, error) {
	var oldest *time.Time
	err := pool.QueryRow(ctx,
		`SELECT min(occurred_at) FROM platform.outbox WHERE published_at IS NULL AND poisoned_at IS NULL`).Scan(&oldest)
	if err != nil {
		return 0, fmt.Errorf("outbox: oldest pending: %w", err)
	}
	if oldest == nil || now.Before(*oldest) {
		return 0, nil
	}
	return now.Sub(*oldest), nil
}
