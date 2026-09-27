// Package application holds the profile module's use cases (see docs/modules.md).
package application

import (
	"context"

	"github.com/aedst/server/internal/modules/profile/domain"
	"github.com/aedst/server/internal/shared/ids"
)

type Repository interface {
	Upsert(ctx context.Context, profile domain.Profile) (domain.Profile, error)
	FindByUserID(ctx context.Context, userID ids.ID) (domain.Profile, error)
}

// EventPublisher publishes profile's domain events (see docs/events.md).
type EventPublisher interface {
	PublishProfileUpdated(ctx context.Context, profile domain.Profile) error
}
