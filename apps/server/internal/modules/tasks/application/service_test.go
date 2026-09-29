package application_test

import (
	"context"
	"testing"
	"time"

	"github.com/aedst/server/internal/modules/tasks/application"
	"github.com/aedst/server/internal/modules/tasks/domain"
	sharederrors "github.com/aedst/server/internal/shared/errors"
	"github.com/aedst/server/internal/shared/ids"
	sharedtime "github.com/aedst/server/internal/shared/time"
)

// fakeRepository is an in-memory test double for application.Repository,
// demonstrating that use cases are testable without a database (see
// docs/architecture.md — "Most business logic should be tested without
// infrastructure").
type fakeRepository struct {
	byID map[ids.ID]domain.Task
}

func newFakeRepository() *fakeRepository {
	return &fakeRepository{byID: map[ids.ID]domain.Task{}}
}

func (f *fakeRepository) Create(_ context.Context, task domain.Task, _ string) (domain.Task, error) {
	f.byID[task.ID] = task
	return task, nil
}

func (f *fakeRepository) FindByIDAndUserID(_ context.Context, id, userID ids.ID) (domain.Task, error) {
	task, ok := f.byID[id]
	if !ok || task.UserID != userID {
		return domain.Task{}, sharederrors.NotFound("fake.FindByIDAndUserID", "task not found")
	}
	return task, nil
}

func (f *fakeRepository) Update(_ context.Context, task domain.Task) (domain.Task, error) {
	existing, ok := f.byID[task.ID]
	if !ok || existing.UserID != task.UserID {
		return domain.Task{}, sharederrors.NotFound("fake.Update", "task not found")
	}
	f.byID[task.ID] = task
	return task, nil
}

func (f *fakeRepository) SetStatus(_ context.Context, id, userID ids.ID, status domain.Status, completedAt *time.Time, updatedAt time.Time) (domain.Task, error) {
	task, ok := f.byID[id]
	if !ok || task.UserID != userID {
		return domain.Task{}, sharederrors.NotFound("fake.SetStatus", "task not found")
	}
	task.Status = status
	task.CompletedAt = completedAt
	task.UpdatedAt = updatedAt
	f.byID[id] = task
	return task, nil
}

func (f *fakeRepository) Complete(_ context.Context, id, userID ids.ID, completedAt, updatedAt time.Time) (domain.Task, error) {
	return f.SetStatus(context.Background(), id, userID, domain.StatusDone, &completedAt, updatedAt)
}

func (f *fakeRepository) Delete(_ context.Context, id, userID ids.ID) error {
	task, ok := f.byID[id]
	if !ok || task.UserID != userID {
		return sharederrors.NotFound("fake.Delete", "task not found")
	}
	delete(f.byID, id)
	return nil
}

func (f *fakeRepository) List(_ context.Context, params application.ListParams) ([]domain.Task, int, error) {
	var matched []domain.Task
	for _, task := range f.byID {
		if task.UserID == params.UserID {
			matched = append(matched, task)
		}
	}
	return matched, len(matched), nil
}

func (f *fakeRepository) Stats(_ context.Context, userID ids.ID) (application.Stats, error) {
	var stats application.Stats
	for _, task := range f.byID {
		if task.UserID != userID {
			continue
		}
		stats.Total++
		switch task.Status {
		case domain.StatusTodo:
			stats.Todo++
		case domain.StatusInProgress:
			stats.InProgress++
		case domain.StatusDone:
			stats.Done++
		}
		if task.IsOverdue(time.Now()) {
			stats.Overdue++
		}
	}
	return stats, nil
}

type fakeReminders struct {
	scheduled int
}

func (f *fakeReminders) ScheduleDueReminder(_ context.Context, _, _ ids.ID, _ time.Time) error {
	f.scheduled++
	return nil
}

func TestService_CreateTask(t *testing.T) {
	repo := newFakeRepository()
	reminders := &fakeReminders{}
	svc := application.NewService(repo, reminders, sharedtime.Fixed{At: time.Now()})

	userID := ids.New()
	due := time.Now().Add(24 * time.Hour)
	task, err := svc.CreateTask(context.Background(), userID, "  Fix bug  ", "details", domain.PriorityHigh, &due, "")
	if err != nil {
		t.Fatalf("CreateTask() error = %v", err)
	}
	if task.Title != "Fix bug" {
		t.Errorf("CreateTask() did not trim title, got %q", task.Title)
	}
	if task.Status != domain.StatusTodo {
		t.Errorf("CreateTask() status = %v, want StatusTodo", task.Status)
	}
	if task.UserID != userID {
		t.Errorf("CreateTask() userID = %v, want %v", task.UserID, userID)
	}
	if reminders.scheduled != 1 {
		t.Errorf("expected a reminder to be scheduled for a task with a due date, got %d schedules", reminders.scheduled)
	}
}

func TestService_CreateTask_NoReminderWithoutDueDate(t *testing.T) {
	repo := newFakeRepository()
	reminders := &fakeReminders{}
	svc := application.NewService(repo, reminders, sharedtime.Fixed{At: time.Now()})

	_, err := svc.CreateTask(context.Background(), ids.New(), "No due date", "", domain.PriorityLow, nil, "")
	if err != nil {
		t.Fatalf("CreateTask() error = %v", err)
	}
	if reminders.scheduled != 0 {
		t.Errorf("expected no reminder for a task without a due date, got %d schedules", reminders.scheduled)
	}
}

// TestService_GetTask_CrossUserAccessIsNotFound is the authorization test:
// a task created by one user must be invisible to another, reported as
// NotFound (see GetTask's doc comment on why NotFound rather than
// PermissionDenied is the correct response here).
func TestService_GetTask_CrossUserAccessIsNotFound(t *testing.T) {
	repo := newFakeRepository()
	svc := application.NewService(repo, &fakeReminders{}, sharedtime.Fixed{At: time.Now()})

	owner := ids.New()
	attacker := ids.New()
	task, err := svc.CreateTask(context.Background(), owner, "Owner's task", "", domain.PriorityMedium, nil, "")
	if err != nil {
		t.Fatalf("CreateTask() error = %v", err)
	}

	if _, err := svc.GetTask(context.Background(), owner, task.ID); err != nil {
		t.Fatalf("owner GetTask() error = %v, want nil", err)
	}

	_, err = svc.GetTask(context.Background(), attacker, task.ID)
	if sharederrors.KindOf(err) != sharederrors.KindNotFound {
		t.Fatalf("attacker GetTask() = %v, want KindNotFound", err)
	}
}

func TestService_UpdateTask_CrossUserAccessIsNotFound(t *testing.T) {
	repo := newFakeRepository()
	svc := application.NewService(repo, &fakeReminders{}, sharedtime.Fixed{At: time.Now()})

	owner := ids.New()
	attacker := ids.New()
	task, err := svc.CreateTask(context.Background(), owner, "Owner's task", "", domain.PriorityMedium, nil, "")
	if err != nil {
		t.Fatalf("CreateTask() error = %v", err)
	}

	_, err = svc.UpdateTask(context.Background(), attacker, task.ID, "hijacked", "", domain.PriorityLow, nil)
	if sharederrors.KindOf(err) != sharederrors.KindNotFound {
		t.Fatalf("attacker UpdateTask() = %v, want KindNotFound", err)
	}

	unchanged, err := svc.GetTask(context.Background(), owner, task.ID)
	if err != nil {
		t.Fatalf("GetTask() after attempted hijack: %v", err)
	}
	if unchanged.Title != "Owner's task" {
		t.Errorf("attacker's UpdateTask mutated the task: title = %q", unchanged.Title)
	}
}

func TestService_DeleteTask_CrossUserAccessIsNotFound(t *testing.T) {
	repo := newFakeRepository()
	svc := application.NewService(repo, &fakeReminders{}, sharedtime.Fixed{At: time.Now()})

	owner := ids.New()
	attacker := ids.New()
	task, err := svc.CreateTask(context.Background(), owner, "Owner's task", "", domain.PriorityMedium, nil, "")
	if err != nil {
		t.Fatalf("CreateTask() error = %v", err)
	}

	if err := svc.DeleteTask(context.Background(), attacker, task.ID); sharederrors.KindOf(err) != sharederrors.KindNotFound {
		t.Fatalf("attacker DeleteTask() = %v, want KindNotFound", err)
	}
	if _, err := svc.GetTask(context.Background(), owner, task.ID); err != nil {
		t.Fatalf("task should still exist after attacker's failed delete, GetTask() error = %v", err)
	}
}

func TestService_CompleteTask_ThenReopen(t *testing.T) {
	repo := newFakeRepository()
	svc := application.NewService(repo, &fakeReminders{}, sharedtime.Fixed{At: time.Now()})

	userID := ids.New()
	task, err := svc.CreateTask(context.Background(), userID, "Task", "", domain.PriorityLow, nil, "")
	if err != nil {
		t.Fatalf("CreateTask() error = %v", err)
	}

	completed, err := svc.CompleteTask(context.Background(), userID, task.ID)
	if err != nil {
		t.Fatalf("CompleteTask() error = %v", err)
	}
	if completed.Status != domain.StatusDone {
		t.Errorf("CompleteTask() status = %v, want StatusDone", completed.Status)
	}
	if completed.CompletedAt == nil {
		t.Error("CompleteTask() did not set CompletedAt")
	}

	reopened, err := svc.ReopenTask(context.Background(), userID, task.ID)
	if err != nil {
		t.Fatalf("ReopenTask() error = %v", err)
	}
	if reopened.Status != domain.StatusTodo {
		t.Errorf("ReopenTask() status = %v, want StatusTodo", reopened.Status)
	}
	if reopened.CompletedAt != nil {
		t.Error("ReopenTask() did not clear CompletedAt")
	}
}

func TestService_ListTasks_DefaultsPageSize(t *testing.T) {
	repo := newFakeRepository()
	svc := application.NewService(repo, &fakeReminders{}, sharedtime.Fixed{At: time.Now()})

	userID := ids.New()
	for i := range 3 {
		if _, err := svc.CreateTask(context.Background(), userID, "Task", "", domain.PriorityLow, nil, ""); err != nil {
			t.Fatalf("CreateTask() #%d error = %v", i, err)
		}
	}

	tasks, total, err := svc.ListTasks(context.Background(), application.ListParams{UserID: userID})
	if err != nil {
		t.Fatalf("ListTasks() error = %v", err)
	}
	if total != 3 || len(tasks) != 3 {
		t.Errorf("ListTasks() = %d tasks (total %d), want 3", len(tasks), total)
	}

	otherUser := ids.New()
	tasks, _, err = svc.ListTasks(context.Background(), application.ListParams{UserID: otherUser})
	if err != nil {
		t.Fatalf("ListTasks() for other user error = %v", err)
	}
	if len(tasks) != 0 {
		t.Errorf("ListTasks() leaked another user's tasks: got %d", len(tasks))
	}
}
