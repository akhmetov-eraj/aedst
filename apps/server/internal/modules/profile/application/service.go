package application

import (
	"context"

	"github.com/aedst/server/internal/modules/profile/domain"
	"github.com/aedst/server/internal/shared/ids"
	sharedtime "github.com/aedst/server/internal/shared/time"
)

type Service struct {
	repo   Repository
	events EventPublisher
	clock  sharedtime.Clock
}

func NewService(repo Repository, events EventPublisher, clock sharedtime.Clock) *Service {
	return &Service{repo: repo, events: events, clock: clock}
}

// CreateDefault creates the initial (empty) profile for a newly registered
// user. Called by the module's UserRegistered consumer (see
// internal/modules/profile/events), not by a synchronous cross-module call —
// see docs/modules.md on why notification-style modules stay decoupled.
func (s *Service) CreateDefault(ctx context.Context, userID ids.ID, displayName string) (domain.Profile, error) {
	return s.repo.Upsert(ctx, domain.Profile{
		UserID:      userID,
		DisplayName: displayName,
		UpdatedAt:   s.clock.Now(),
	})
}

func (s *Service) UpdateProfile(ctx context.Context, userID ids.ID, displayName, bio, avatarURL string) (domain.Profile, error) {
	profile, err := s.repo.Upsert(ctx, domain.Profile{
		UserID:      userID,
		DisplayName: displayName,
		Bio:         bio,
		AvatarURL:   avatarURL,
		UpdatedAt:   s.clock.Now(),
	})
	if err != nil {
		return domain.Profile{}, err
	}

	if err := s.events.PublishProfileUpdated(ctx, profile); err != nil {
		return domain.Profile{}, err
	}

	return profile, nil
}

func (s *Service) GetProfile(ctx context.Context, userID ids.ID) (domain.Profile, error) {
	return s.repo.FindByUserID(ctx, userID)
}
