// Package inbox implements the consumer-side counterpart to
// internal/platform/outbox: it turns the at-least-once delivery that NATS
// JetStream (redelivery on Nak/timeout) and a crashed-mid-processing
// consumer both imply into effectively-once processing, by checking and
// recording "already handled" atomically with the handler's own database
// side effect, in one transaction. The handler is still expected to be
// idempotent on its own as defense in depth (see docs/events.md) — this
// package makes that defense unnecessary in the common case, not optional
// in the rare one.
package inbox

import (
	"context"

	"github.com/jackc/pgx/v5"

	"github.com/aedst/server/internal/shared/ids"
)

// Repository tracks which event ids a consumer has already processed.
// Implemented per module under infrastructure/postgres, backed by that
// module's own schema.
type Repository interface {
	IsProcessed(ctx context.Context, tx pgx.Tx, eventID ids.ID) (bool, error)
	MarkProcessed(ctx context.Context, tx pgx.Tx, eventID ids.ID) error
}
