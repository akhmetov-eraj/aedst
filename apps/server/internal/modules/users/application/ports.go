// Package application holds the users module's use cases. It is allowed to
// depend on modules/auth/api (users depends on auth — see docs/modules.md)
// but on nothing else outside this module and shared/.
package application

import (
	"context"

	"github.com/aedst/server/internal/modules/users/domain"
	"github.com/aedst/server/internal/shared/ids"
)

type Repository interface {
	Create(ctx context.Context, user domain.User) error
	FindByID(ctx context.Context, id ids.ID) (domain.User, error)
	FindByEmail(ctx context.Context, email string) (domain.User, error)
}

// EventPublisher publishes users' domain events (see docs/events.md).
type EventPublisher interface {
	PublishUserRegistered(ctx context.Context, user domain.User) error
}
