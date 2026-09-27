package events

import (
	"context"
	"fmt"

	"github.com/nats-io/nats.go/jetstream"
	"github.com/riverqueue/river"
	"github.com/riverqueue/river/rivertype"
	"google.golang.org/protobuf/proto"

	usersv1 "github.com/aedst/server/gen/go/aedst/users/v1"
	"github.com/aedst/server/internal/modules/profile/domain"
	profilejobs "github.com/aedst/server/internal/modules/profile/jobs"
	"github.com/aedst/server/internal/platform/messaging"
	"github.com/aedst/server/internal/shared/ids"
)

// DefaultProfileCreator is the subset of profile's application.Service the
// UserRegistered consumer needs.
type DefaultProfileCreator interface {
	CreateDefault(ctx context.Context, userID ids.ID, displayName string) (domain.Profile, error)
}

// JobInserter is the subset of *jobs.Client (see platform/jobs) the
// UserRegistered consumer needs to enqueue the welcome-email job. Kept as a
// minimal interface so the handler is testable without a real River client
// or Postgres pool.
type JobInserter interface {
	Insert(ctx context.Context, args river.JobArgs, opts *river.InsertOpts) (*rivertype.JobInsertResult, error)
}

const usersConsumerDurable = "profile-on-user-registered"

// ConsumeUserRegistered subscribes to users' UserRegistered event (published
// on the AEDST_USERS stream, see modules/users/events) and creates a default
// profile for each newly registered user. Blocks until ctx is cancelled; run
// it in its own goroutine from cmd/server.
func ConsumeUserRegistered(ctx context.Context, client *messaging.Client, creator DefaultProfileCreator, jobs JobInserter) error {
	return client.Subscribe(ctx, "AEDST_USERS", usersConsumerDurable, "aedst.users.user_registered.v1", func(ctx context.Context, msg jetstream.Msg) error {
		return HandleUserRegistered(ctx, msg.Data(), creator, jobs)
	})
}

// HandleUserRegistered contains the per-message logic of ConsumeUserRegistered,
// pulled out of the NATS subscription wrapper so it can be unit tested with
// fakes instead of a running NATS/JetStream server (see
// modules/profile/events/consumer_test.go).
func HandleUserRegistered(ctx context.Context, data []byte, creator DefaultProfileCreator, jobs JobInserter) error {
	var event usersv1.UserRegistered
	if err := proto.Unmarshal(data, &event); err != nil {
		return fmt.Errorf("profile: decode UserRegistered: %w", err)
	}

	userID, err := ids.Parse(event.GetUserId())
	if err != nil {
		return fmt.Errorf("profile: invalid user_id in UserRegistered: %w", err)
	}

	if _, err := creator.CreateDefault(ctx, userID, ""); err != nil {
		return err
	}

	if jobs == nil {
		return nil
	}

	if _, err := jobs.Insert(ctx, profilejobs.WelcomeEmailArgs{
		UserID: userID.String(),
		Email:  event.GetEmail(),
	}, nil); err != nil {
		return fmt.Errorf("profile: enqueue welcome email job: %w", err)
	}

	return nil
}
