// Package domain holds the tasks module's entities. It imports nothing
// outside the standard library and shared primitives — no infrastructure,
// no other module (see docs/modules.md).
package domain

import (
	"time"

	"github.com/aedst/server/internal/shared/ids"
)

type Status string

const (
	StatusTodo       Status = "todo"
	StatusInProgress Status = "in_progress"
	StatusDone       Status = "done"
)

type Priority string

const (
	PriorityLow    Priority = "low"
	PriorityMedium Priority = "medium"
	PriorityHigh   Priority = "high"
	PriorityUrgent Priority = "urgent"
)

// Task is owned entirely by its creator — see
// api/proto/aedst/tasks/v1/tasks.proto's service doc comment for why
// nothing in this module ever takes a user id from a request.
type Task struct {
	ID          ids.ID
	UserID      ids.ID
	Title       string
	Description string
	Status      Status
	Priority    Priority
	DueDate     *time.Time
	CompletedAt *time.Time
	CreatedAt   time.Time
	UpdatedAt   time.Time
}

func (t Task) IsOverdue(now time.Time) bool {
	return t.Status != StatusDone && t.DueDate != nil && t.DueDate.Before(now)
}
