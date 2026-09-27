// Package api is the auth module's public boundary. Other modules (today:
// users) and cmd/server's transport wiring may import this package; nothing
// else in modules/auth may be imported from outside the module (see
// docs/architecture.md and docs/decisions/0006-module-boundaries.md).
package api

import (
	"context"

	"github.com/aedst/server/internal/modules/auth/application"
	"github.com/aedst/server/internal/shared/ids"
)

// TokenPair is the DTO other modules and transport receive; it mirrors
// application.TokenPair so callers never import the application package
// directly.
type TokenPair = application.TokenPair

// Service is everything another module or a transport handler can do with
// auth. It is satisfied by *application.Service.
type Service interface {
	// CreateCredential and IssueSession are called by users.RegisterUser
	// in-process; they are not exposed over the public ConnectRPC surface.
	CreateCredential(ctx context.Context, userID ids.ID, email, password string) error
	IssueSession(ctx context.Context, userID ids.ID, deviceID string) (TokenPair, error)

	Login(ctx context.Context, email, password, deviceID string) (ids.ID, TokenPair, error)
	RefreshToken(ctx context.Context, rawRefreshToken string) (TokenPair, error)
	Logout(ctx context.Context, rawRefreshToken string) error
}

var _ Service = (*application.Service)(nil)
