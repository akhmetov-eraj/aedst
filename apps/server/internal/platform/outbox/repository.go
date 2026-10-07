package outbox

import (
	"context"
	"time"

	"github.com/jackc/pgx/v5"

	"github.com/aedst/server/internal/shared/ids"
)

// Backlog is a snapshot of one module's undelivered rows, for the lag metrics.
type Backlog struct {
	// Pending is the number of rows still owed delivery, capped by the
	// implementation (counting an unbounded backlog would itself be the slow
	// query) — treat it as "at least this many" when it equals BacklogCap.
	Pending int64
	// Oldest is when the oldest pending row committed; zero when none are
	// pending. now-Oldest is the delivery lag.
	Oldest time.Time
}

// BacklogCap bounds Backlog.Pending.
const BacklogCap = 10_000

// Repository persists outbox rows. Insert runs inside the same transaction
// as the domain write that produced the event; every other method runs
// standalone against the module's pool, driven by Relay. Implemented per
// module under infrastructure/postgres, backed by that module's own schema —
// outbox rows never cross module boundaries (see docs/content/database.md).
type Repository interface {
	// Insert writes row inside tx, atomically with the domain change that
	// produced it.
	Insert(ctx context.Context, tx pgx.Tx, row Row) error

	// Claim leases up to limit deliverable rows (unpublished, not poisoned,
	// retry time reached) oldest first, so concurrent relays — other server
	// replicas — never take the same row (FOR UPDATE SKIP LOCKED). Each
	// claimed row has its Attempts incremented and is invisible to other
	// claims for lease; a relay that crashes mid-delivery simply lets the
	// lease expire and the row is claimed again.
	Claim(ctx context.Context, limit int, lease time.Duration) ([]Row, error)

	// MarkPublished records that eventIDs were delivered to all subscribers.
	MarkPublished(ctx context.Context, eventIDs []ids.ID) error

	// Retry releases the lease on eventID and makes it claimable again at
	// retryAt (exponential backoff is the relay's decision, not the store's).
	Retry(ctx context.Context, eventID ids.ID, retryAt time.Time) error

	// MarkPoison flags eventID as permanently undeliverable; the row is kept
	// for inspection and replay.
	MarkPoison(ctx context.Context, eventID ids.ID) error

	// Backlog reports how many rows are pending and how old the oldest is.
	Backlog(ctx context.Context) (Backlog, error)

	// PurgePublished deletes up to limit rows published before cutoff and
	// returns how many it deleted, so the table stays bounded.
	PurgePublished(ctx context.Context, cutoff time.Time, limit int) (int64, error)

	// Get loads one row (inspection/replay tooling and tests). Returns a
	// NotFound-kind error if it no longer exists.
	Get(ctx context.Context, eventID ids.ID) (Row, error)
}
