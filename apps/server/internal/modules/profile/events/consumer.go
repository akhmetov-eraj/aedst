package events

import (
	"context"
	"fmt"

	"github.com/nats-io/nats.go/jetstream"
	"google.golang.org/protobuf/proto"

	usersv1 "github.com/aedst/server/gen/go/aedst/users/v1"
	"github.com/aedst/server/internal/modules/profile/domain"
	"github.com/aedst/server/internal/platform/messaging"
	"github.com/aedst/server/internal/shared/ids"
)

// DefaultProfileCreator is the subset of profile's application.Service the
// UserRegistered consumer needs.
type DefaultProfileCreator interface {
	CreateDefault(ctx context.Context, userID ids.ID, displayName string) (domain.Profile, error)
}

const usersConsumerDurable = "profile-on-user-registered"

// ConsumeUserRegistered subscribes to users' UserRegistered event (published
// on the AEDST_USERS stream, see modules/users/events) and creates a default
// profile for each newly registered user. Blocks until ctx is cancelled; run
// it in its own goroutine from cmd/server.
func ConsumeUserRegistered(ctx context.Context, client *messaging.Client, creator DefaultProfileCreator) error {
	return client.Subscribe(ctx, "AEDST_USERS", usersConsumerDurable, "aedst.users.user_registered.v1", func(ctx context.Context, msg jetstream.Msg) error {
		var event usersv1.UserRegistered
		if err := proto.Unmarshal(msg.Data(), &event); err != nil {
			return fmt.Errorf("profile: decode UserRegistered: %w", err)
		}

		userID, err := ids.Parse(event.GetUserId())
		if err != nil {
			return fmt.Errorf("profile: invalid user_id in UserRegistered: %w", err)
		}

		_, err = creator.CreateDefault(ctx, userID, "")
		return err
	})
}
