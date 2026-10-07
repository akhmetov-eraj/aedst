// Package application holds the tasks module's use cases. It depends only
// on domain and the interfaces declared here — infrastructure/postgres
// provides the concrete implementation, keeping this package testable
// without a database or a job queue (see docs/modules.md).
package application

import (
	"context"
	"time"

	"github.com/aedst/server/internal/modules/tasks/domain"
	"github.com/aedst/server/internal/shared/ids"
	"github.com/aedst/server/internal/shared/pagination"
)

type Repository interface {
	// Create persists task. If idempotencyKey is non-empty, the write is
	// wrapped in an idempotency check (see internal/platform/idempotency) so
	// a client retrying the same CreateTask request after a dropped response
	// gets back the original task instead of a duplicate. Side effects that
	// must not be lost (the due-date reminder job) commit in the same
	// transaction.
	Create(ctx context.Context, task domain.Task, idempotencyKey string) (domain.Task, error)
	FindByIDAndUserID(ctx context.Context, id, userID ids.ID) (domain.Task, error)
	// Update applies params.Patch to title/description/priority/due_date only
	// — see tasks.proto's UpdateTask doc comment on why status is never
	// included — in one atomic statement. It returns a Conflict-kind error
	// when params.ExpectedVersion is set and stale, NotFound when the task is
	// not the caller's.
	Update(ctx context.Context, params UpdateParams) (domain.Task, error)
	// Complete moves id to done and, in the same transaction, writes a
	// TaskCompleted outbox row (see internal/platform/outbox). Completing an
	// already-done task returns it as-is and writes nothing: the transition,
	// and the event, happen at most once.
	Complete(ctx context.Context, id, userID ids.ID, now time.Time) (domain.Task, error)
	// Reopen moves id back to todo; a no-op when it already is.
	Reopen(ctx context.Context, id, userID ids.ID, now time.Time) (domain.Task, error)
	Delete(ctx context.Context, id, userID ids.ID) error
	List(ctx context.Context, params ListParams) (ListResult, error)
	Stats(ctx context.Context, userID ids.ID) (Stats, error)
}

// UpdateParams is one partial update of a task.
type UpdateParams struct {
	ID     ids.ID
	UserID ids.ID
	Patch  domain.Patch
	// ExpectedVersion, when non-nil, makes the update conditional on the task
	// still being at that version (optimistic concurrency).
	ExpectedVersion *int
	UpdatedAt       time.Time
}

// ListParams selects one keyset page of a user's tasks (see
// internal/shared/pagination). After is the cursor decoded from the client's
// page token, nil for the first page.
type ListParams struct {
	UserID        ids.ID
	Statuses      []domain.Status
	Priorities    []domain.Priority
	Search        string
	SortBy        string // "created_at" | "due_date"
	SortDirection string // "asc" | "desc"
	Limit         int
	After         *pagination.Cursor
}

// SortToken names the ordering of a request. It is embedded in cursors so a
// token cannot be replayed against a different ordering.
func (p ListParams) SortToken() string { return p.SortBy + ":" + p.SortDirection }

// Normalize applies the defaults and bounds for unset or out-of-range fields.
// Call it before SortToken so the token is stable whether or not the client
// spelled out the default ordering.
func (p *ListParams) Normalize() {
	if p.Limit <= 0 {
		p.Limit = defaultPageSize
	}
	if p.Limit > maxPageSize {
		p.Limit = maxPageSize
	}
	if p.SortBy == "" {
		p.SortBy = "created_at"
	}
	if p.SortDirection == "" {
		p.SortDirection = "desc"
	}
}

// ListResult is one page. Total is the count of tasks matching the filter and
// is populated only on the first page (After == nil); on later pages it is 0
// because counting the matching set is O(rows) and keyset paging exists to
// avoid per-page O(rows) work.
type ListResult struct {
	Tasks []domain.Task
	Next  *pagination.Cursor
	Total int
}

type Stats struct {
	Total      int
	Todo       int
	InProgress int
	Done       int
	Overdue    int
}
