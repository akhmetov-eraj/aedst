// Package events implements profile's EventPublisher port and its
// UserRegistered consumer on top of platform/messaging (see docs/events.md).
package events

import (
	"context"
	"time"

	"google.golang.org/protobuf/types/known/timestamppb"

	profilev1 "github.com/aedst/server/gen/go/aedst/profile/v1"
	"github.com/aedst/server/internal/modules/profile/domain"
	"github.com/aedst/server/internal/platform/messaging"
	"github.com/aedst/server/internal/shared/ids"
)

const (
	Stream                = "AEDST_PROFILE"
	SubjectProfileUpdated = "aedst.profile.profile_updated.v1"
)

func Subjects() []string {
	return []string{"aedst.profile.>"}
}

type Publisher struct {
	client *messaging.Client
}

func NewPublisher(client *messaging.Client) *Publisher {
	return &Publisher{client: client}
}

func (p *Publisher) PublishProfileUpdated(ctx context.Context, profile domain.Profile) error {
	return p.client.Publish(ctx, SubjectProfileUpdated, &profilev1.ProfileUpdated{
		EventId:     ids.New().String(),
		UserId:      profile.UserID.String(),
		DisplayName: profile.DisplayName,
		AvatarUrl:   profile.AvatarURL,
		OccurredAt:  timestamppb.New(time.Now().UTC()),
	})
}
