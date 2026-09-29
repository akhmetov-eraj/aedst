package application

import (
	"context"
	"strings"
	"time"

	"github.com/aedst/server/internal/modules/tasks/domain"
	"github.com/aedst/server/internal/shared/ids"
	sharedtime "github.com/aedst/server/internal/shared/time"
)

const (
	defaultPageSize = 20
	maxPageSize     = 100
)

type Service struct {
	repo      Repository
	reminders ReminderScheduler
	clock     sharedtime.Clock
}

func NewService(repo Repository, reminders ReminderScheduler, clock sharedtime.Clock) *Service {
	return &Service{repo: repo, reminders: reminders, clock: clock}
}

func (s *Service) CreateTask(ctx context.Context, userID ids.ID, title, description string, priority domain.Priority, dueDate *time.Time, idempotencyKey string) (domain.Task, error) {
	now := s.clock.Now()
	task := domain.Task{
		ID:          ids.New(),
		UserID:      userID,
		Title:       strings.TrimSpace(title),
		Description: description,
		Status:      domain.StatusTodo,
		Priority:    priority,
		DueDate:     dueDate,
		CreatedAt:   now,
		UpdatedAt:   now,
	}

	created, err := s.repo.Create(ctx, task, idempotencyKey)
	if err != nil {
		return domain.Task{}, err
	}

	s.scheduleReminder(ctx, created)
	return created, nil
}

// GetTask returns NotFound (never PermissionDenied) when id belongs to
// another user. Unlike a request that names another user's id directly
// (see e.g. Profile's UpdateProfile ownership check), nothing here ever
// takes a user id from the client — the only way to even attempt reaching
// another user's task is to guess a UUID, and the repository query scopes
// by the authenticated user's id already. Reporting NotFound rather than
// PermissionDenied for that case avoids confirming to an unauthorized
// caller that the id they guessed exists at all (see OWASP's guidance on
// preferring 404 over 403 for object-level access checks).
func (s *Service) GetTask(ctx context.Context, userID, id ids.ID) (domain.Task, error) {
	return s.repo.FindByIDAndUserID(ctx, id, userID)
}

func (s *Service) ListTasks(ctx context.Context, params ListParams) ([]domain.Task, int, error) {
	if params.PageSize <= 0 {
		params.PageSize = defaultPageSize
	}
	if params.PageSize > maxPageSize {
		params.PageSize = maxPageSize
	}
	if params.SortBy == "" {
		params.SortBy = "created_at"
	}
	if params.SortDirection == "" {
		params.SortDirection = "desc"
	}
	return s.repo.List(ctx, params)
}

func (s *Service) UpdateTask(ctx context.Context, userID, id ids.ID, title, description string, priority domain.Priority, dueDate *time.Time) (domain.Task, error) {
	existing, err := s.repo.FindByIDAndUserID(ctx, id, userID)
	if err != nil {
		return domain.Task{}, err
	}

	existing.Title = strings.TrimSpace(title)
	existing.Description = description
	existing.Priority = priority
	existing.DueDate = dueDate
	existing.UpdatedAt = s.clock.Now()

	updated, err := s.repo.Update(ctx, existing)
	if err != nil {
		return domain.Task{}, err
	}

	s.scheduleReminder(ctx, updated)
	return updated, nil
}

func (s *Service) DeleteTask(ctx context.Context, userID, id ids.ID) error {
	// Confirms ownership first so a delete of someone else's (or a
	// nonexistent) task id reports NotFound, not a silent no-op — a caller
	// that thinks it deleted a task it never owned is a worse failure mode
	// than an explicit error.
	if _, err := s.repo.FindByIDAndUserID(ctx, id, userID); err != nil {
		return err
	}
	return s.repo.Delete(ctx, id, userID)
}

func (s *Service) CompleteTask(ctx context.Context, userID, id ids.ID) (domain.Task, error) {
	now := s.clock.Now()
	return s.repo.Complete(ctx, id, userID, now, now)
}

func (s *Service) ReopenTask(ctx context.Context, userID, id ids.ID) (domain.Task, error) {
	return s.repo.SetStatus(ctx, id, userID, domain.StatusTodo, nil, s.clock.Now())
}

func (s *Service) GetStats(ctx context.Context, userID ids.ID) (Stats, error) {
	return s.repo.Stats(ctx, userID)
}

// scheduleReminder enqueues the due-reminder job when a task has a future
// due date. Best-effort: a scheduling failure doesn't fail the
// create/update that triggered it — see docs/events.md's precedent for the
// same trade-off (a missed reminder is recoverable; a create/update that
// looks like it failed after the row already committed is a worse UX).
func (s *Service) scheduleReminder(ctx context.Context, task domain.Task) {
	if task.DueDate == nil || task.Status == domain.StatusDone {
		return
	}
	_ = s.reminders.ScheduleDueReminder(ctx, task.ID, task.UserID, *task.DueDate)
}
