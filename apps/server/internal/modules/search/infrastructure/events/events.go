// Package events builds the search projection from other modules'
// integration events (tasks, media), each applied inside an inbox
// transaction on the jobs pool. Publishers are reached only through their
// api/ packages; ordering is handled by version (newest wins, tombstones).
package events

import (
	"context"
	"encoding/json"
	"fmt"
	"log/slog"
	"time"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"

	mediaapi "github.com/aedst/server/internal/modules/media/api"
	"github.com/aedst/server/internal/modules/search/domain"
	tasksapi "github.com/aedst/server/internal/modules/tasks/api"
	"github.com/aedst/server/internal/platform/inbox"
	"github.com/aedst/server/internal/platform/messaging"
	"github.com/aedst/server/internal/shared/ids"
)

// Projection is the write side of the search repository.
type Projection interface {
	Upsert(ctx context.Context, tx pgx.Tx, d domain.Document) error
	Tombstone(ctx context.Context, tx pgx.Tx, kind domain.Kind, entityID, ownerID ids.ID, version int, at time.Time) error
}

// Subscribe registers the search projections on bus.
func Subscribe(bus *messaging.Bus, pool *pgxpool.Pool, proj Projection, logger *slog.Logger) {
	snapshot := func(ctx context.Context, tx pgx.Tx, _ ids.ID, payload []byte) error {
		var evt tasksapi.TaskSnapshot
		if err := json.Unmarshal(payload, &evt); err != nil {
			return fmt.Errorf("decode task snapshot: %w", err)
		}
		taskID, owner, err := parseIDs(evt.TaskID, evt.UserID)
		if err != nil {
			return err
		}
		return proj.Upsert(ctx, tx, domain.Document{
			Kind: domain.KindTask, EntityID: taskID, OwnerID: owner,
			Title: evt.Title, Body: evt.Description, Link: "/tasks/" + taskID.String(),
			Version: evt.Version, UpdatedAt: evt.UpdatedAt,
		})
	}
	subscribe(bus, pool, logger, tasksapi.TaskCreatedEventType, "search.task_created", snapshot)
	subscribe(bus, pool, logger, tasksapi.TaskUpdatedEventType, "search.task_updated", snapshot)
	subscribe(bus, pool, logger, tasksapi.TaskDeletedEventType, "search.task_deleted", func(ctx context.Context, tx pgx.Tx, _ ids.ID, payload []byte) error {
		var evt tasksapi.TaskDeletedEvent
		if err := json.Unmarshal(payload, &evt); err != nil {
			return fmt.Errorf("decode %s: %w", tasksapi.TaskDeletedEventType, err)
		}
		taskID, owner, err := parseIDs(evt.TaskID, evt.UserID)
		if err != nil {
			return err
		}
		return proj.Tombstone(ctx, tx, domain.KindTask, taskID, owner, evt.Version, evt.DeletedAt)
	})
	subscribe(bus, pool, logger, mediaapi.MediaReadyEventType, "search.media_ready", func(ctx context.Context, tx pgx.Tx, _ ids.ID, payload []byte) error {
		var evt mediaapi.MediaReadyEvent
		if err := json.Unmarshal(payload, &evt); err != nil {
			return fmt.Errorf("decode %s: %w", mediaapi.MediaReadyEventType, err)
		}
		mediaID, owner, err := parseIDs(evt.MediaID, evt.UserID)
		if err != nil {
			return err
		}
		return proj.Upsert(ctx, tx, domain.Document{
			Kind: domain.KindMedia, EntityID: mediaID, OwnerID: owner,
			Title: evt.Filename, Link: "/media/" + mediaID.String(),
			Version: evt.Version, UpdatedAt: evt.ReadyAt,
		})
	})
	subscribe(bus, pool, logger, mediaapi.MediaDeletedEventType, "search.media_deleted", func(ctx context.Context, tx pgx.Tx, _ ids.ID, payload []byte) error {
		var evt mediaapi.MediaDeletedEvent
		if err := json.Unmarshal(payload, &evt); err != nil {
			return fmt.Errorf("decode %s: %w", mediaapi.MediaDeletedEventType, err)
		}
		mediaID, owner, err := parseIDs(evt.MediaID, evt.UserID)
		if err != nil {
			return err
		}
		return proj.Tombstone(ctx, tx, domain.KindMedia, mediaID, owner, evt.Version, evt.DeletedAt)
	})
}

func subscribe(bus *messaging.Bus, pool *pgxpool.Pool, logger *slog.Logger, eventType, consumer string, handle inbox.Handler) {
	c := inbox.NewConsumer(consumer, pool, handle, logger)
	bus.Subscribe(eventType, consumer, c.Subscriber())
}

func parseIDs(entity, owner string) (ids.ID, ids.ID, error) {
	e, err := ids.Parse(entity)
	if err != nil {
		return ids.Nil, ids.Nil, fmt.Errorf("entity id: %w", err)
	}
	o, err := ids.Parse(owner)
	if err != nil {
		return ids.Nil, ids.Nil, fmt.Errorf("owner id: %w", err)
	}
	return e, o, nil
}
