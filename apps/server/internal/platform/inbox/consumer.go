package inbox

import (
	"context"
	"fmt"
	"log/slog"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"

	"github.com/aedst/server/internal/platform/database"
	"github.com/aedst/server/internal/shared/ids"
)

// Handler applies one already-deduped event inside tx. Returning an error
// rolls back the transaction (so MarkProcessed never commits) and leaves the
// message to be redelivered.
type Handler func(ctx context.Context, tx pgx.Tx, eventID ids.ID, payload []byte) error

// Consumer subscribes to one subject, dedupes on the outbox event id, and
// applies Handler exactly once per event id, regardless of redelivery.
type Consumer struct {
	module string
	pool   *pgxpool.Pool
	repo   Repository
	handle Handler
	logger *slog.Logger
}

func NewConsumer(module string, pool *pgxpool.Pool, repo Repository, handle Handler, logger *slog.Logger) *Consumer {
	return &Consumer{module: module, pool: pool, repo: repo, handle: handle, logger: logger}
}

// Process is a messaging.HandlerFunc-shaped entry point: call it from the
// subject's Subscribe handler, passing the event id parsed from the
// message's messaging.HeaderEventID header.
func (c *Consumer) Process(ctx context.Context, eventID ids.ID, payload []byte) error {
	err := database.WithTx(ctx, c.pool, func(ctx context.Context, tx pgx.Tx) error {
		processed, err := c.repo.IsProcessed(ctx, tx, eventID)
		if err != nil {
			return fmt.Errorf("inbox: check processed: %w", err)
		}
		if processed {
			c.logger.DebugContext(ctx, "inbox: duplicate delivery, skipping",
				slog.String("module", c.module), slog.String("event_id", eventID.String()))
			return nil
		}

		if err := c.handle(ctx, tx, eventID, payload); err != nil {
			return fmt.Errorf("inbox: handle: %w", err)
		}
		return c.repo.MarkProcessed(ctx, tx, eventID)
	})
	if err != nil {
		return fmt.Errorf("inbox: process %s: %w", eventID, err)
	}
	return nil
}
