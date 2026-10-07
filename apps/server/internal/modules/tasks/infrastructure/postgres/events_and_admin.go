package postgres

import (
	"context"
	"encoding/json"
	"errors"
	"time"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgtype"

	tasksdb "github.com/aedst/server/gen/db/tasks"
	"github.com/aedst/server/internal/modules/tasks/api"
	"github.com/aedst/server/internal/modules/tasks/application"
	"github.com/aedst/server/internal/modules/tasks/domain"
	"github.com/aedst/server/internal/platform/database"
	"github.com/aedst/server/internal/platform/outbox"
	sharederrors "github.com/aedst/server/internal/shared/errors"
	"github.com/aedst/server/internal/shared/ids"
	"github.com/aedst/server/internal/shared/pagination"
)

// writeEvent records an integration event inside tx (the transactional
// outbox). A repository built without an outbox writer cannot mutate.
func (r *Repository) writeEvent(ctx context.Context, tx pgx.Tx, eventType string, version uint32, key ids.ID, payload any) error {
	if r.outbox == nil {
		return sharederrors.Wrap("tasks.writeEvent", errors.New("repository has no outbox writer"))
	}
	body, err := json.Marshal(payload)
	if err != nil {
		return sharederrors.Wrap("tasks.writeEvent", err)
	}
	return r.outbox.Write(ctx, tx, outbox.NewRow(ctx, r.clock, eventType, version, key, body))
}

// Update applies the patch in one statement and, in the same transaction,
// publishes TaskUpdated with the post-update snapshot (and enqueues the
// reminder when the due date changed). When the UPDATE matches no row, one
// follow-up read tells "not found" from "stale version"; nothing is published.
func (r *Repository) Update(ctx context.Context, params application.UpdateParams) (domain.Task, error) {
	var result domain.Task
	err := database.WithTx(ctx, r.pool, func(ctx context.Context, tx pgx.Tx) error {
		q := tasksdb.New(tx)
		row, err := q.UpdateTask(ctx, updateParams(params))
		if errors.Is(err, pgx.ErrNoRows) {
			if _, findErr := q.FindTaskByIDAndUserID(ctx, tasksdb.FindTaskByIDAndUserIDParams{ID: toPgUUID(params.ID), UserID: toPgUUID(params.UserID)}); findErr == nil {
				return sharederrors.Conflict("tasks.Update", "task was modified by someone else; reload and retry")
			}
			return sharederrors.NotFound("tasks.Update", "task not found")
		}
		if err != nil {
			return sharederrors.Wrap("tasks.Update", err)
		}
		result = fromDB(row)
		if params.Patch.SetDueDate {
			if err := r.enqueueReminder(ctx, tx, result); err != nil {
				return err
			}
		}
		return r.writeEvent(ctx, tx, api.TaskUpdatedEventType, api.TaskUpdatedEventVersion, result.ID, api.SnapshotOf(result))
	})
	if err != nil {
		return domain.Task{}, err
	}
	return result, nil
}

// Delete removes the caller's task and publishes TaskDeleted in the same
// transaction. Nothing matched (someone else's or a nonexistent id) is
// NotFound and publishes nothing.
func (r *Repository) Delete(ctx context.Context, id, userID ids.ID) error {
	return database.WithTx(ctx, r.pool, func(ctx context.Context, tx pgx.Tx) error {
		version, err := tasksdb.New(tx).DeleteTask(ctx, tasksdb.DeleteTaskParams{ID: toPgUUID(id), UserID: toPgUUID(userID)})
		if errors.Is(err, pgx.ErrNoRows) {
			return sharederrors.NotFound("tasks.Delete", "task not found")
		}
		if err != nil {
			return sharederrors.Wrap("tasks.Delete", err)
		}
		return r.writeDeleted(ctx, tx, id, userID, int(version))
	})
}

func (r *Repository) writeDeleted(ctx context.Context, tx pgx.Tx, id, userID ids.ID, version int) error {
	return r.writeEvent(ctx, tx, api.TaskDeletedEventType, api.TaskDeletedEventVersion, id, api.TaskDeletedEvent{
		TaskID:    id.String(),
		UserID:    userID.String(),
		DeletedAt: r.clock.Now(),
		Version:   version + 1,
	})
}

// MarkDueSoon publishes TaskDueSoon for (id, dueDate) at most once: the guarded
// insert into tasks.due_soon_marks returns a row only the first time and only
// while the task is open and still due then. It reports whether it published.
func (r *Repository) MarkDueSoon(ctx context.Context, id, userID ids.ID, dueDate time.Time) (bool, error) {
	published := false
	err := database.WithTx(ctx, r.pool, func(ctx context.Context, tx pgx.Tx) error {
		q := tasksdb.New(tx)
		_, err := q.MarkDueSoon(ctx, tasksdb.MarkDueSoonParams{ID: toPgUUID(id), UserID: toPgUUID(userID), DueDate: toPgTimestamptz(dueDate)})
		if errors.Is(err, pgx.ErrNoRows) {
			return nil // already published, done, deleted, or due date moved
		}
		if err != nil {
			return sharederrors.Wrap("tasks.MarkDueSoon", err)
		}
		row, err := q.FindTaskByIDAndUserID(ctx, tasksdb.FindTaskByIDAndUserIDParams{ID: toPgUUID(id), UserID: toPgUUID(userID)})
		if err != nil {
			return sharederrors.Wrap("tasks.MarkDueSoon", err)
		}
		published = true
		return r.writeEvent(ctx, tx, api.TaskDueSoonEventType, api.TaskDueSoonEventVersion, id, api.TaskDueSoonEvent{
			TaskID:     id.String(),
			UserID:     userID.String(),
			Title:      row.Title,
			DueDate:    dueDate,
			OccurredAt: r.clock.Now(),
		})
	})
	if err != nil {
		return false, err
	}
	return published, nil
}

// --- operator (admin) operations: not scoped to an owner ---

// AdminList pages every owner's tasks (or one owner's), newest first.
func (r *Repository) AdminList(ctx context.Context, params application.AdminListParams) (application.ListResult, error) {
	cursorKey, cursorID := openCursor(false)
	if params.After != nil {
		cursorKey = pgtype.Timestamptz{Time: params.After.Key, Valid: true}
		cursorID = toPgUUID(params.After.ID)
	}
	var owner pgtype.UUID
	if params.OwnerID != nil {
		owner = toPgUUID(*params.OwnerID)
	}
	rows, err := tasksdb.New(r.pool).AdminListTasks(ctx, tasksdb.AdminListTasksParams{
		OwnerID:   owner,
		Search:    escapeLike(params.Search),
		CursorKey: cursorKey,
		CursorID:  cursorID,
		PageLimit: int32(params.Limit) + 1,
	})
	if err != nil {
		return application.ListResult{}, sharederrors.Wrap("tasks.AdminList", err)
	}
	tasks := make([]domain.Task, len(rows))
	for i, row := range rows {
		tasks[i] = fromDB(row)
	}
	page := pagination.NewPage(tasks, params.Limit, func(t domain.Task) pagination.Cursor {
		return pagination.Cursor{Sort: application.AdminSortToken, Key: t.CreatedAt, ID: t.ID}
	})
	return application.ListResult{Tasks: page.Items, Next: page.Next}, nil
}

// AdminDeleteTx deletes any task inside the caller's transaction (the admin
// module's audit transaction) and publishes TaskDeleted in it. It returns the
// deleted task's owner.
func (r *Repository) AdminDeleteTx(ctx context.Context, tx pgx.Tx, id ids.ID) (ids.ID, error) {
	row, err := tasksdb.New(tx).AdminDeleteTask(ctx, toPgUUID(id))
	if errors.Is(err, pgx.ErrNoRows) {
		return ids.Nil, sharederrors.NotFound("tasks.AdminDelete", "task not found")
	}
	if err != nil {
		return ids.Nil, sharederrors.Wrap("tasks.AdminDelete", err)
	}
	owner := fromPgUUID(row.UserID)
	return owner, r.writeDeleted(ctx, tx, id, owner, int(row.Version))
}

func (r *Repository) AdminStats(ctx context.Context) (application.AdminStats, error) {
	row, err := tasksdb.New(r.pool).AdminTaskStats(ctx)
	if err != nil {
		return application.AdminStats{}, sharederrors.Wrap("tasks.AdminStats", err)
	}
	return application.AdminStats{Total: row.Total, Done: row.Done, Overdue: row.Overdue}, nil
}

func (r *Repository) CountByOwners(ctx context.Context, owners []ids.ID) (map[ids.ID]int64, error) {
	pg := make([]pgtype.UUID, len(owners))
	for i, o := range owners {
		pg[i] = toPgUUID(o)
	}
	rows, err := tasksdb.New(r.pool).CountTasksByOwners(ctx, pg)
	if err != nil {
		return nil, sharederrors.Wrap("tasks.CountByOwners", err)
	}
	out := make(map[ids.ID]int64, len(rows))
	for _, row := range rows {
		out[fromPgUUID(row.UserID)] = row.TaskCount
	}
	return out, nil
}
