// Package jobs holds profile's River job kinds (deferred work owned solely by
// this module, see docs/decisions/0005-river-for-jobs.md — distinct from the
// cross-module notifications in modules/profile/events). Its
// WelcomeEmailWorker is registered with the shared River client in
// cmd/server and enqueued by profile's UserRegistered consumer.
package jobs

import (
	"context"
	"log/slog"

	"github.com/riverqueue/river"
)

// WelcomeEmailArgs is the payload for the welcome-email job enqueued after a
// new user registers (see modules/profile/events.ConsumeUserRegistered).
type WelcomeEmailArgs struct {
	UserID string `json:"user_id"`
	Email  string `json:"email"`
}

// Kind implements river.JobArgs.
func (WelcomeEmailArgs) Kind() string { return "profile.welcome_email" }

// WelcomeEmailWorker sends (today: logs) the welcome email for a newly
// registered user. Sending real email is out of scope for this proof of the
// River wiring — see ADR-0005; a future change swaps the body of Work for a
// real mail-provider call without touching how the job is enqueued.
type WelcomeEmailWorker struct {
	river.WorkerDefaults[WelcomeEmailArgs]

	Logger *slog.Logger
}

func NewWelcomeEmailWorker(logger *slog.Logger) *WelcomeEmailWorker {
	return &WelcomeEmailWorker{Logger: logger}
}

func (w *WelcomeEmailWorker) Work(ctx context.Context, job *river.Job[WelcomeEmailArgs]) error {
	w.Logger.InfoContext(ctx, "sending welcome email",
		slog.String("user_id", job.Args.UserID),
		slog.String("email", job.Args.Email),
	)
	return nil
}
