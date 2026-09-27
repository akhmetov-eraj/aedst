// Package events implements users' EventPublisher port on top of
// platform/messaging, translating domain.User into the versioned proto event
// (see docs/events.md).
package events

import (
	"context"
	"time"

	"google.golang.org/protobuf/types/known/timestamppb"

	usersv1 "github.com/aedst/server/gen/go/aedst/users/v1"
	"github.com/aedst/server/internal/modules/users/domain"
	"github.com/aedst/server/internal/platform/messaging"
	"github.com/aedst/server/internal/shared/ids"
)

const (
	Stream                = "AEDST_USERS"
	SubjectUserRegistered = "aedst.users.user_registered.v1"
)

func Subjects() []string {
	return []string{"aedst.users.>"}
}

type Publisher struct {
	client *messaging.Client
}

func NewPublisher(client *messaging.Client) *Publisher {
	return &Publisher{client: client}
}

func (p *Publisher) PublishUserRegistered(ctx context.Context, user domain.User) error {
	return p.client.Publish(ctx, SubjectUserRegistered, &usersv1.UserRegistered{
		EventId:    ids.New().String(),
		UserId:     user.ID.String(),
		Email:      user.Email,
		OccurredAt: timestamppb.New(time.Now().UTC()),
	})
}
