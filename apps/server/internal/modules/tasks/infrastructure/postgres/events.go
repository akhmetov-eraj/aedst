package postgres

import "time"

// TaskCompletedEventType/Version identify the tasks.v1.TaskCompleted
// integration event published via the outbox on task completion (see
// Repository.Complete and docs/events.md). JSON, not a protobuf contract —
// see internal/platform/outbox.JSONEnvelope's doc comment for why: this
// module has no other integration-event consumer yet, so a protobuf
// contracts/ package isn't warranted until one exists.
const (
	TaskCompletedEventType    = "tasks.v1.TaskCompleted"
	taskCompletedEventVersion = 1
)

// TaskCompletedEvent is the outbox payload for TaskCompletedEventType.
type TaskCompletedEvent struct {
	TaskID      string    `json:"task_id"`
	UserID      string    `json:"user_id"`
	CompletedAt time.Time `json:"completed_at"`
}
