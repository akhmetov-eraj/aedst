// Package application holds the auth module's use cases. It depends only on
// domain and the interfaces declared here — infrastructure/postgres provides
// the concrete implementations, keeping this package testable without a
// database (see docs/modules.md).
package application

import (
	"context"
	"time"

	"github.com/aedst/server/internal/modules/auth/domain"
	"github.com/aedst/server/internal/shared/ids"
)

type CredentialRepository interface {
	Create(ctx context.Context, cred domain.Credential) error
	FindByEmail(ctx context.Context, email string) (domain.Credential, error)
	FindByUserID(ctx context.Context, userID ids.ID) (domain.Credential, error)
}

type SessionRepository interface {
	Create(ctx context.Context, session domain.Session) error
	FindByID(ctx context.Context, id ids.ID) (domain.Session, error)
	Rotate(ctx context.Context, id ids.ID, newRefreshTokenHash string, newExpiresAt time.Time) error
	Revoke(ctx context.Context, id ids.ID, revokedAt time.Time) error
}

// TokenIssuer is satisfied by platform/auth.TokenService. Declaring the
// interface here (instead of importing the concrete type) keeps application
// decoupled from the jwx-specific implementation.
type TokenIssuer interface {
	IssuePair(userID, sessionID ids.ID, deviceID string, roles []string) (TokenPair, error)
	VerifyRefreshToken(ctx context.Context, raw string) (RefreshClaims, error)
}

type TokenPair struct {
	AccessToken  string
	RefreshToken string
	AccessTTL    time.Duration
	RefreshTTL   time.Duration
}

type RefreshClaims struct {
	UserID    ids.ID
	SessionID ids.ID
}

// PasswordHasher hashes and verifies passwords (bcrypt in production).
type PasswordHasher interface {
	Hash(password string) (string, error)
	Verify(hash, password string) bool
}

// User is the identity DTO Register/GetCurrentUser return — deliberately not
// domain.Credential (which carries PasswordHash): api/api.go type-aliases
// this, never domain types, so a password hash can never leak across the
// module boundary by accident.
type User struct {
	ID        ids.ID
	Email     string
	CreatedAt time.Time
}
