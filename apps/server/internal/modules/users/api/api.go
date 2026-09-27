// Package api is the users module's public boundary (see docs/modules.md and
// docs/decisions/0006-module-boundaries.md).
package api

import (
	"context"

	authapi "github.com/aedst/server/internal/modules/auth/api"
	"github.com/aedst/server/internal/modules/users/application"
	"github.com/aedst/server/internal/modules/users/domain"
	"github.com/aedst/server/internal/shared/ids"
)

// User is the DTO other modules receive.
type User = domain.User

// TokenPair is re-exported from auth/api so callers of RegisterUser don't
// need to import auth themselves.
type TokenPair = authapi.TokenPair

type Service interface {
	RegisterUser(ctx context.Context, email, password, deviceID string) (User, TokenPair, error)
	GetUser(ctx context.Context, id ids.ID) (User, error)
	GetUserByEmail(ctx context.Context, email string) (User, error)
}

var _ Service = (*application.Service)(nil)
