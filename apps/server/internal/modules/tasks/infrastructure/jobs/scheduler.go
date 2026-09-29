// Package jobs implements the tasks module's ReminderScheduler port on top
// of platform/jobs' shared River client (see
// docs/decisions/0005-river-for-jobs.md). Not imported outside this module.
package jobs

import (
	"context"
	"time"

	"github.com/riverqueue/river"
	"github.com/riverqueue/river/rivertype"

	tasksjobs "github.com/aedst/server/internal/modules/tasks/jobs"
	"github.com/aedst/server/internal/shared/ids"
)

// JobInserter is the subset of *platform/jobs.Client the scheduler needs —
// kept minimal so tests can fake it without a real River client or Postgres
// pool (same pattern as profile's former events.JobInserter).
type JobInserter interface {
	Insert(ctx context.Context, args river.JobArgs, opts *river.InsertOpts) (*rivertype.JobInsertResult, error)
}

type Scheduler struct {
	jobs JobInserter
}

func NewScheduler(jobs JobInserter) *Scheduler {
	return &Scheduler{jobs: jobs}
}

func (s *Scheduler) ScheduleDueReminder(ctx context.Context, taskID, userID ids.ID, dueDate time.Time) error {
	_, err := s.jobs.Insert(ctx, tasksjobs.DueReminderArgs{
		TaskID:  taskID.String(),
		UserID:  userID.String(),
		DueDate: dueDate.Unix(),
	}, nil)
	return err
}
