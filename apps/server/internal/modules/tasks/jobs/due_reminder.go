// Package jobs holds tasks' River job kinds (deferred work owned solely by
// this module, see docs/decisions/0005-river-for-jobs.md). DueReminderWorker
// is registered with the shared River client in cmd/server and enqueued by
// application.Service whenever a task's due date is set (see
// application/service.go's scheduleReminder).
package jobs

import (
	"context"
	"log/slog"
	"time"

	"github.com/riverqueue/river"

	"github.com/aedst/server/internal/modules/tasks/domain"
	sharederrors "github.com/aedst/server/internal/shared/errors"
	"github.com/aedst/server/internal/shared/ids"
)

// TaskFinder is the subset of application.Repository the reminder worker
// needs — kept minimal so tests can fake it without a real Repository.
type TaskFinder interface {
	FindByIDAndUserID(ctx context.Context, id, userID ids.ID) (domain.Task, error)
}

// DueReminderArgs is the payload for the reminder job enqueued when a task
// gets a due date (see application.Service.scheduleReminder).
type DueReminderArgs struct {
	TaskID  string `json:"task_id"`
	UserID  string `json:"user_id"`
	DueDate int64  `json:"due_date"` // Unix seconds, the due_date this job was scheduled for.
}

func (DueReminderArgs) Kind() string { return "tasks.due_reminder" }

// InsertOpts schedules the job to run at the task's due date rather than
// immediately — River runs it no earlier than ScheduledAt, not necessarily
// exactly then (subject to worker availability), which is the right
// trade-off for a reminder.
func (a DueReminderArgs) InsertOpts() river.InsertOpts {
	return river.InsertOpts{ScheduledAt: time.Unix(a.DueDate, 0).UTC()}
}

// DueReminderWorker sends (today: logs) a reminder for a task whose due date
// has arrived. Sending a real push/email notification is out of scope for
// this proof of the River wiring, same as profile's former welcome-email
// worker this replaces — see ADR-0005.
type DueReminderWorker struct {
	river.WorkerDefaults[DueReminderArgs]

	Tasks  TaskFinder
	Logger *slog.Logger
}

func NewDueReminderWorker(tasks TaskFinder, logger *slog.Logger) *DueReminderWorker {
	return &DueReminderWorker{Tasks: tasks, Logger: logger}
}

func (w *DueReminderWorker) Work(ctx context.Context, job *river.Job[DueReminderArgs]) error {
	taskID, err := ids.Parse(job.Args.TaskID)
	if err != nil {
		return err
	}
	userID, err := ids.Parse(job.Args.UserID)
	if err != nil {
		return err
	}

	task, err := w.Tasks.FindByIDAndUserID(ctx, taskID, userID)
	if err != nil {
		if sharederrors.KindOf(err) == sharederrors.KindNotFound {
			// Deleted since this job was scheduled — nothing to remind
			// about. Not an error: see ReminderScheduler's doc comment on
			// why stale jobs are expected to no-op instead of being
			// actively cancelled.
			return nil
		}
		return err
	}

	if task.Status == domain.StatusDone {
		w.Logger.InfoContext(ctx, "skipping due reminder: task already done", slog.String("task_id", taskID.String()))
		return nil
	}
	if task.DueDate == nil || task.DueDate.Unix() != job.Args.DueDate {
		// The due date changed after this job was scheduled (e.g. via
		// UpdateTask, which schedules a fresh job for the new date) — this
		// is the stale copy; the fresh one will fire at the right time.
		w.Logger.InfoContext(ctx, "skipping due reminder: due date changed since scheduling", slog.String("task_id", taskID.String()))
		return nil
	}

	w.Logger.InfoContext(ctx, "task due reminder",
		slog.String("task_id", taskID.String()),
		slog.String("user_id", userID.String()),
		slog.String("title", task.Title),
	)
	return nil
}
