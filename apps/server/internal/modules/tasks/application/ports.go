// Package application holds the tasks module's use cases. It depends only
// on domain and the interfaces declared here — infrastructure/postgres and
// infrastructure/jobs provide the concrete implementations, keeping this
// package testable without a database or a job queue (see docs/modules.md).
package application

import (
	"context"
	"time"

	"github.com/aedst/server/internal/modules/tasks/domain"
	"github.com/aedst/server/internal/shared/ids"
)

type Repository interface {
	// Create persists task. If idempotencyKey is non-empty, the write is
	// wrapped in an idempotency check (see internal/platform/idempotency) so
	// a client retrying the same CreateTask request after a dropped response
	// gets back the original task instead of a duplicate.
	Create(ctx context.Context, task domain.Task, idempotencyKey string) (domain.Task, error)
	FindByIDAndUserID(ctx context.Context, id, userID ids.ID) (domain.Task, error)
	// Update mutates title/description/priority/due_date only — see
	// tasks.proto's UpdateTask doc comment on why status is never included.
	Update(ctx context.Context, task domain.Task) (domain.Task, error)
	// SetStatus is used by ReopenTask only. CompleteTask uses Complete
	// instead, which additionally publishes a TaskCompleted event via the
	// outbox in the same transaction — see docs/events.md.
	SetStatus(ctx context.Context, id, userID ids.ID, status domain.Status, completedAt *time.Time, updatedAt time.Time) (domain.Task, error)
	// Complete marks id done and, in the same transaction, writes a
	// TaskCompleted outbox row (see internal/platform/outbox).
	Complete(ctx context.Context, id, userID ids.ID, completedAt, updatedAt time.Time) (domain.Task, error)
	Delete(ctx context.Context, id, userID ids.ID) error
	List(ctx context.Context, params ListParams) ([]domain.Task, int, error)
	Stats(ctx context.Context, userID ids.ID) (Stats, error)
}

type ListParams struct {
	UserID        ids.ID
	Statuses      []domain.Status
	Priorities    []domain.Priority
	Search        string
	SortBy        string // "created_at" | "due_date"
	SortDirection string // "asc" | "desc"
	PageSize      int
	Offset        int
}

type Stats struct {
	Total      int
	Todo       int
	InProgress int
	Done       int
	Overdue    int
}

// ReminderScheduler enqueues the due-date reminder job (see
// internal/modules/tasks/jobs and docs/decisions/0005-river-for-jobs.md) —
// kept as its own port, separate from Repository, so Service stays testable
// with a fake instead of a real job queue.
type ReminderScheduler interface {
	// ScheduleDueReminder is a no-op if dueDate is nil. Safe to call
	// repeatedly for the same task (e.g. on every UpdateTask that touches
	// due_date) — DueReminderWorker re-checks the task's current due_date
	// against what it was scheduled for and no-ops on mismatch, so a stale
	// job from a since-changed or since-completed task never sends a wrong
	// reminder; see jobs/due_reminder.go.
	ScheduleDueReminder(ctx context.Context, taskID, userID ids.ID, dueDate time.Time) error
}
