// Package postgres implements the tasks module's Repository against the
// sqlc-generated queries in gen/db/tasks. Not imported outside this module.
package postgres

import (
	"context"
	"encoding/json"
	"errors"
	"time"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgtype"
	"github.com/jackc/pgx/v5/pgxpool"

	tasksdb "github.com/aedst/server/gen/db/tasks"
	"github.com/aedst/server/internal/modules/tasks/application"
	"github.com/aedst/server/internal/modules/tasks/domain"
	"github.com/aedst/server/internal/platform/database"
	"github.com/aedst/server/internal/platform/idempotency"
	"github.com/aedst/server/internal/platform/outbox"
	sharederrors "github.com/aedst/server/internal/shared/errors"
	"github.com/aedst/server/internal/shared/ids"
	sharedtime "github.com/aedst/server/internal/shared/time"
)

type Repository struct {
	pool        *pgxpool.Pool
	outbox      outbox.Repository
	idempotency idempotency.Store
	clock       sharedtime.Clock
}

func NewRepository(pool *pgxpool.Pool, outboxRepo outbox.Repository, idempotencyStore idempotency.Store, clock sharedtime.Clock) *Repository {
	return &Repository{pool: pool, outbox: outboxRepo, idempotency: idempotencyStore, clock: clock}
}

// Create persists task. When idempotencyKey is non-empty, the write and the
// idempotency-key record commit in the same transaction (see
// internal/platform/idempotency), so a client retry after a dropped
// response replays the original result instead of creating a duplicate task.
func (r *Repository) Create(ctx context.Context, task domain.Task, idempotencyKey string) (domain.Task, error) {
	// Hashed on the client-supplied fields only — never task.ID/CreatedAt,
	// which Service.CreateTask generates fresh on every call including
	// retries, and would otherwise make every replay look like a
	// different request (see idempotency.Check's doc comment on request
	// hash mismatch).
	requestPayload, err := json.Marshal(struct {
		UserID      ids.ID          `json:"user_id"`
		Title       string          `json:"title"`
		Description string          `json:"description"`
		Priority    domain.Priority `json:"priority"`
		DueDate     *time.Time      `json:"due_date"`
	}{task.UserID, task.Title, task.Description, task.Priority, task.DueDate})
	if err != nil {
		return domain.Task{}, sharederrors.Wrap("tasks.Create", err)
	}

	var result domain.Task
	err = database.WithTx(ctx, r.pool, func(ctx context.Context, tx pgx.Tx) error {
		if cached, replay, err := idempotency.Check(ctx, tx, r.idempotency, idempotencyKey, requestPayload); err != nil {
			return err
		} else if replay {
			return json.Unmarshal(cached, &result)
		}

		q := tasksdb.New(tx)
		row, err := q.CreateTask(ctx, tasksdb.CreateTaskParams{
			ID:          toPgUUID(task.ID),
			UserID:      toPgUUID(task.UserID),
			Title:       task.Title,
			Description: task.Description,
			Priority:    toDBPriority(task.Priority),
			DueDate:     toPgTimestamptzPtr(task.DueDate),
			CreatedAt:   toPgTimestamptz(task.CreatedAt),
		})
		if err != nil {
			return sharederrors.Wrap("tasks.Create", err)
		}
		result = fromDB(row)

		resultPayload, err := json.Marshal(result)
		if err != nil {
			return sharederrors.Wrap("tasks.Create", err)
		}
		return idempotency.Save(ctx, tx, r.idempotency, idempotencyKey, requestPayload, resultPayload)
	})
	if err != nil {
		return domain.Task{}, err
	}
	return result, nil
}

// Complete marks id done and, in the same transaction, writes a
// TaskCompleted event to the outbox (see internal/platform/outbox) —
// CompleteTask's fire-and-forget precedent doesn't apply here: unlike a
// best-effort reminder, an integration event another system's projection
// depends on (see docs/events.md) must not be silently lost.
func (r *Repository) Complete(ctx context.Context, id, userID ids.ID, completedAt, updatedAt time.Time) (domain.Task, error) {
	var result domain.Task
	err := database.WithTx(ctx, r.pool, func(ctx context.Context, tx pgx.Tx) error {
		q := tasksdb.New(tx)
		row, err := q.SetTaskStatus(ctx, tasksdb.SetTaskStatusParams{
			ID:          toPgUUID(id),
			UserID:      toPgUUID(userID),
			Status:      toDBStatus(domain.StatusDone),
			CompletedAt: toPgTimestamptzPtr(&completedAt),
			UpdatedAt:   toPgTimestamptz(updatedAt),
		})
		if err != nil {
			if errors.Is(err, pgx.ErrNoRows) {
				return sharederrors.NotFound("tasks.Complete", "task not found")
			}
			return sharederrors.Wrap("tasks.Complete", err)
		}
		result = fromDB(row)

		payload, err := json.Marshal(TaskCompletedEvent{
			TaskID:      result.ID.String(),
			UserID:      result.UserID.String(),
			CompletedAt: completedAt,
		})
		if err != nil {
			return sharederrors.Wrap("tasks.Complete", err)
		}
		outboxRow := outbox.NewRow(r.clock, TaskCompletedEventType, taskCompletedEventVersion, result.ID, "", payload)
		return r.outbox.Insert(ctx, tx, outboxRow)
	})
	if err != nil {
		return domain.Task{}, err
	}
	return result, nil
}

func (r *Repository) FindByIDAndUserID(ctx context.Context, id, userID ids.ID) (domain.Task, error) {
	q := tasksdb.New(r.pool)
	row, err := q.FindTaskByIDAndUserID(ctx, tasksdb.FindTaskByIDAndUserIDParams{
		ID:     toPgUUID(id),
		UserID: toPgUUID(userID),
	})
	if err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return domain.Task{}, sharederrors.NotFound("tasks.FindByIDAndUserID", "task not found")
		}
		return domain.Task{}, sharederrors.Wrap("tasks.FindByIDAndUserID", err)
	}
	return fromDB(row), nil
}

func (r *Repository) Update(ctx context.Context, task domain.Task) (domain.Task, error) {
	q := tasksdb.New(r.pool)
	row, err := q.UpdateTask(ctx, tasksdb.UpdateTaskParams{
		ID:          toPgUUID(task.ID),
		UserID:      toPgUUID(task.UserID),
		Title:       task.Title,
		Description: task.Description,
		Priority:    toDBPriority(task.Priority),
		DueDate:     toPgTimestamptzPtr(task.DueDate),
		UpdatedAt:   toPgTimestamptz(task.UpdatedAt),
	})
	if err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return domain.Task{}, sharederrors.NotFound("tasks.Update", "task not found")
		}
		return domain.Task{}, sharederrors.Wrap("tasks.Update", err)
	}
	return fromDB(row), nil
}

func (r *Repository) SetStatus(ctx context.Context, id, userID ids.ID, status domain.Status, completedAt *time.Time, updatedAt time.Time) (domain.Task, error) {
	q := tasksdb.New(r.pool)
	row, err := q.SetTaskStatus(ctx, tasksdb.SetTaskStatusParams{
		ID:          toPgUUID(id),
		UserID:      toPgUUID(userID),
		Status:      toDBStatus(status),
		CompletedAt: toPgTimestamptzPtr(completedAt),
		UpdatedAt:   toPgTimestamptz(updatedAt),
	})
	if err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return domain.Task{}, sharederrors.NotFound("tasks.SetStatus", "task not found")
		}
		return domain.Task{}, sharederrors.Wrap("tasks.SetStatus", err)
	}
	return fromDB(row), nil
}

func (r *Repository) Delete(ctx context.Context, id, userID ids.ID) error {
	q := tasksdb.New(r.pool)
	affected, err := q.DeleteTask(ctx, tasksdb.DeleteTaskParams{ID: toPgUUID(id), UserID: toPgUUID(userID)})
	if err != nil {
		return sharederrors.Wrap("tasks.Delete", err)
	}
	if affected == 0 {
		return sharederrors.NotFound("tasks.Delete", "task not found")
	}
	return nil
}

func (r *Repository) List(ctx context.Context, params application.ListParams) ([]domain.Task, int, error) {
	q := tasksdb.New(r.pool)
	rows, err := q.ListTasks(ctx, tasksdb.ListTasksParams{
		UserID:        toPgUUID(params.UserID),
		Statuses:      toDBStatuses(params.Statuses),
		Priorities:    toDBPriorities(params.Priorities),
		Search:        params.Search,
		SortBy:        params.SortBy,
		SortDirection: params.SortDirection,
		PageLimit:     int32(params.PageSize),
		PageOffset:    int32(params.Offset),
	})
	if err != nil {
		return nil, 0, sharederrors.Wrap("tasks.List", err)
	}

	tasks := make([]domain.Task, 0, len(rows))
	total := 0
	for _, row := range rows {
		tasks = append(tasks, fromDB(tasksdb.TasksTask{
			ID:          row.ID,
			UserID:      row.UserID,
			Title:       row.Title,
			Description: row.Description,
			Status:      row.Status,
			Priority:    row.Priority,
			DueDate:     row.DueDate,
			CompletedAt: row.CompletedAt,
			CreatedAt:   row.CreatedAt,
			UpdatedAt:   row.UpdatedAt,
		}))
		total = int(row.TotalCount)
	}
	return tasks, total, nil
}

func (r *Repository) Stats(ctx context.Context, userID ids.ID) (application.Stats, error) {
	q := tasksdb.New(r.pool)
	row, err := q.GetTaskStats(ctx, toPgUUID(userID))
	if err != nil {
		return application.Stats{}, sharederrors.Wrap("tasks.Stats", err)
	}
	return application.Stats{
		Total:      int(row.Total),
		Todo:       int(row.Todo),
		InProgress: int(row.InProgress),
		Done:       int(row.Done),
		Overdue:    int(row.Overdue),
	}, nil
}

func fromDB(row tasksdb.TasksTask) domain.Task {
	return domain.Task{
		ID:          fromPgUUID(row.ID),
		UserID:      fromPgUUID(row.UserID),
		Title:       row.Title,
		Description: row.Description,
		Status:      fromDBStatus(row.Status),
		Priority:    fromDBPriority(row.Priority),
		DueDate:     fromPgTimestamptzPtr(row.DueDate),
		CompletedAt: fromPgTimestamptzPtr(row.CompletedAt),
		CreatedAt:   row.CreatedAt.Time,
		UpdatedAt:   row.UpdatedAt.Time,
	}
}

func toPgUUID(id ids.ID) pgtype.UUID {
	return pgtype.UUID{Bytes: id, Valid: !id.IsNil()}
}

func fromPgUUID(u pgtype.UUID) ids.ID {
	return ids.ID(u.Bytes)
}

func toPgTimestamptz(t time.Time) pgtype.Timestamptz {
	return pgtype.Timestamptz{Time: t, Valid: !t.IsZero()}
}

func toPgTimestamptzPtr(t *time.Time) pgtype.Timestamptz {
	if t == nil {
		return pgtype.Timestamptz{}
	}
	return pgtype.Timestamptz{Time: *t, Valid: true}
}

func fromPgTimestamptzPtr(t pgtype.Timestamptz) *time.Time {
	if !t.Valid {
		return nil
	}
	return &t.Time
}

func toDBStatus(s domain.Status) tasksdb.TasksStatus         { return tasksdb.TasksStatus(s) }
func fromDBStatus(s tasksdb.TasksStatus) domain.Status       { return domain.Status(s) }
func toDBPriority(p domain.Priority) tasksdb.TasksPriority   { return tasksdb.TasksPriority(p) }
func fromDBPriority(p tasksdb.TasksPriority) domain.Priority { return domain.Priority(p) }

func toDBStatuses(statuses []domain.Status) []tasksdb.TasksStatus {
	out := make([]tasksdb.TasksStatus, len(statuses))
	for i, s := range statuses {
		out[i] = toDBStatus(s)
	}
	return out
}

func toDBPriorities(priorities []domain.Priority) []tasksdb.TasksPriority {
	out := make([]tasksdb.TasksPriority, len(priorities))
	for i, p := range priorities {
		out[i] = toDBPriority(p)
	}
	return out
}
