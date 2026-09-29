// Package api is the tasks module's public boundary. Only cmd/server's
// transport wiring imports this package; nothing else in modules/tasks may
// be imported from outside the module (see docs/architecture.md and
// docs/decisions/0006-module-boundaries.md). Nothing outside this module
// needs tasks today — it has no cross-module consumers — but the api/
// package exists anyway so a future module (e.g. a notifications module
// wanting task counts) has the one sanctioned way in, consistent with
// every other module.
package api

import (
	"context"
	"time"

	"github.com/aedst/server/internal/modules/tasks/application"
	"github.com/aedst/server/internal/modules/tasks/domain"
	"github.com/aedst/server/internal/shared/ids"
)

type (
	Task       = domain.Task
	Status     = domain.Status
	Priority   = domain.Priority
	ListParams = application.ListParams
	Stats      = application.Stats
)

const (
	StatusTodo       = domain.StatusTodo
	StatusInProgress = domain.StatusInProgress
	StatusDone       = domain.StatusDone

	PriorityLow    = domain.PriorityLow
	PriorityMedium = domain.PriorityMedium
	PriorityHigh   = domain.PriorityHigh
	PriorityUrgent = domain.PriorityUrgent
)

// Service is everything a transport handler can do with tasks. It is
// satisfied by *application.Service. Every method takes userID explicitly,
// derived by the caller from the authenticated context — see
// api/proto/aedst/tasks/v1/tasks.proto's service doc comment.
type Service interface {
	// CreateTask persists a new task. idempotencyKey, if non-empty, lets a
	// client-retried request return the originally created task instead of
	// creating a duplicate — see internal/platform/idempotency.
	CreateTask(ctx context.Context, userID ids.ID, title, description string, priority Priority, dueDate *time.Time, idempotencyKey string) (Task, error)
	GetTask(ctx context.Context, userID, id ids.ID) (Task, error)
	ListTasks(ctx context.Context, params ListParams) ([]Task, int, error)
	UpdateTask(ctx context.Context, userID, id ids.ID, title, description string, priority Priority, dueDate *time.Time) (Task, error)
	DeleteTask(ctx context.Context, userID, id ids.ID) error
	CompleteTask(ctx context.Context, userID, id ids.ID) (Task, error)
	ReopenTask(ctx context.Context, userID, id ids.ID) (Task, error)
	GetStats(ctx context.Context, userID ids.ID) (Stats, error)
}

var _ Service = (*application.Service)(nil)
