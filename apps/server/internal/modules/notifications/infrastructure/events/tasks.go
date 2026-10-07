// Package events subscribes the notifications module to integration events
// published by other modules. It depends on a publisher module only through
// that module's api/ package (the event contract), never its internals.
// Each projection runs inside an inbox transaction on the jobs pool, so the
// notification, its push job and the "event applied" marker commit together
// and a redelivered event changes nothing.
package events

import (
	"context"
	"encoding/json"
	"fmt"
	"log/slog"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"

	mediaapi "github.com/aedst/server/internal/modules/media/api"
	tasksapi "github.com/aedst/server/internal/modules/tasks/api"

	"github.com/aedst/server/internal/modules/notifications/domain"
	"github.com/aedst/server/internal/platform/inbox"
	"github.com/aedst/server/internal/platform/messaging"
	"github.com/aedst/server/internal/shared/ids"
)

// Consumer names: the bus subscription name and the platform.inbox key.
const (
	taskCompletedConsumer = "notifications.task_completed"
	taskDueSoonConsumer   = "notifications.task_due_soon"
	mediaReadyConsumer    = "notifications.media_ready"
)

// Inserter stores a notification inside the inbox transaction. Satisfied by
// the module's postgres repository.
type Inserter interface {
	InsertTx(ctx context.Context, tx pgx.Tx, n domain.Notification) error
}

// Subscribe registers every projection of this module on bus.
func Subscribe(bus *messaging.Bus, pool *pgxpool.Pool, repo Inserter, logger *slog.Logger) {
	subscribe(bus, pool, logger, tasksapi.TaskCompletedEventType, taskCompletedConsumer, func(ctx context.Context, tx pgx.Tx, eventID ids.ID, payload []byte) error {
		var evt tasksapi.TaskCompletedEvent
		userID, taskID, err := decode(payload, &evt, tasksapi.TaskCompletedEventType, func() (string, string) { return evt.UserID, evt.TaskID })
		if err != nil {
			return err
		}
		return repo.InsertTx(ctx, tx, domain.NewTaskCompleted(userID, eventID, taskID, evt.Title, evt.CompletedAt))
	})
	subscribe(bus, pool, logger, tasksapi.TaskDueSoonEventType, taskDueSoonConsumer, func(ctx context.Context, tx pgx.Tx, eventID ids.ID, payload []byte) error {
		var evt tasksapi.TaskDueSoonEvent
		userID, taskID, err := decode(payload, &evt, tasksapi.TaskDueSoonEventType, func() (string, string) { return evt.UserID, evt.TaskID })
		if err != nil {
			return err
		}
		createdAt := evt.OccurredAt
		if createdAt.IsZero() {
			createdAt = evt.DueDate
		}
		return repo.InsertTx(ctx, tx, domain.NewTaskDueSoon(userID, eventID, taskID, evt.Title, evt.DueDate, createdAt))
	})
	subscribe(bus, pool, logger, mediaapi.MediaReadyEventType, mediaReadyConsumer, func(ctx context.Context, tx pgx.Tx, eventID ids.ID, payload []byte) error {
		var evt mediaapi.MediaReadyEvent
		userID, mediaID, err := decode(payload, &evt, mediaapi.MediaReadyEventType, func() (string, string) { return evt.UserID, evt.MediaID })
		if err != nil {
			return err
		}
		return repo.InsertTx(ctx, tx, domain.NewMediaReady(userID, eventID, mediaID, evt.Filename, evt.ReadyAt))
	})
}

// SubscribeTaskCompleted is kept for callers (and tests) that only want the
// reference projection.
func SubscribeTaskCompleted(bus *messaging.Bus, pool *pgxpool.Pool, repo Inserter, logger *slog.Logger) {
	subscribe(bus, pool, logger, tasksapi.TaskCompletedEventType, taskCompletedConsumer, func(ctx context.Context, tx pgx.Tx, eventID ids.ID, payload []byte) error {
		var evt tasksapi.TaskCompletedEvent
		userID, taskID, err := decode(payload, &evt, tasksapi.TaskCompletedEventType, func() (string, string) { return evt.UserID, evt.TaskID })
		if err != nil {
			return err
		}
		return repo.InsertTx(ctx, tx, domain.NewTaskCompleted(userID, eventID, taskID, evt.Title, evt.CompletedAt))
	})
}

func subscribe(bus *messaging.Bus, pool *pgxpool.Pool, logger *slog.Logger, eventType, consumer string, handle inbox.Handler) {
	c := inbox.NewConsumer(consumer, pool, handle, logger)
	bus.Subscribe(eventType, consumer, c.Subscriber())
}

// decode unmarshals payload into evt and parses the (user, subject) ids that
// fields returns. A malformed payload fails the delivery (retried, then
// poisoned by the relay) instead of being silently dropped.
func decode(payload []byte, evt any, eventType string, fields func() (string, string)) (ids.ID, ids.ID, error) {
	if err := json.Unmarshal(payload, evt); err != nil {
		return ids.Nil, ids.Nil, fmt.Errorf("decode %s: %w", eventType, err)
	}
	rawUser, rawSubject := fields()
	userID, err := ids.Parse(rawUser)
	if err != nil {
		return ids.Nil, ids.Nil, fmt.Errorf("decode %s: user_id: %w", eventType, err)
	}
	subject, err := ids.Parse(rawSubject)
	if err != nil {
		return ids.Nil, ids.Nil, fmt.Errorf("decode %s: subject id: %w", eventType, err)
	}
	return userID, subject, nil
}
