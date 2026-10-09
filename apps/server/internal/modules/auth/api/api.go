// Package api is the auth module's public boundary. Only cmd/server's
// transport wiring may import this package; nothing else in modules/auth
// may be imported from outside the module (see docs/content/server/architecture.md and
// docs/content/decisions/0006-module-boundaries.md).
package api

import (
	"context"

	"github.com/aedst/server/internal/modules/auth/application"
	"github.com/aedst/server/internal/shared/ids"
	"github.com/aedst/server/internal/shared/pagination"
)

// TokenPair and User are the DTOs transport receives; they mirror
// application.TokenPair/application.User so callers never import the
// application package directly.
type (
	TokenPair = application.TokenPair
	User      = application.User
)

// Service is everything a transport handler can do with auth. It is
// satisfied by *application.Service.
type Service interface {
	Register(ctx context.Context, email, password, deviceID string) (User, TokenPair, error)
	Login(ctx context.Context, email, password, deviceID string) (ids.ID, TokenPair, error)
	RefreshToken(ctx context.Context, rawRefreshToken string) (TokenPair, error)
	Logout(ctx context.Context, rawRefreshToken string) error
	// GetCurrentUser takes the caller's own id (derived server-side from the
	// bearer token, see platform/auth.FromContext) — never a client-supplied
	// one.
	GetCurrentUser(ctx context.Context, userID ids.ID) (User, error)
}

var _ Service = (*application.Service)(nil)

type (
	DirectoryUser = application.DirectoryUser
	UserPage      = application.UserPage
)

// DirectorySortToken binds Directory cursors to their ordering.
const DirectorySortToken = application.DirectorySortToken

// Directory is the operator view of first-party accounts (admin module). It is
// only wired when first-party auth is enabled.
type Directory interface {
	ListUsers(ctx context.Context, search string, limit int32, after *pagination.Cursor) (UserPage, error)
	CountUsers(ctx context.Context) (int64, error)
}

var _ Directory = (*application.Directory)(nil)
