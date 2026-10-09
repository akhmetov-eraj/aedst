// Package application holds the auth module's use cases. It depends only on
// domain and the interfaces declared here — infrastructure/postgres provides
// the concrete implementations, keeping this package testable without a
// database (see docs/content/server/modules.md).
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
	// UpdatePasswordHash replaces the stored hash (transparent upgrade of
	// legacy bcrypt hashes and outdated argon2id parameters on login).
	UpdatePasswordHash(ctx context.Context, userID ids.ID, hash string) error
}

type SessionRepository interface {
	Create(ctx context.Context, session domain.Session) error
	FindByID(ctx context.Context, id ids.ID) (domain.Session, error)
	// Rotate atomically swaps the refresh-token hash if and only if the
	// session is still active AND its current hash equals oldRefreshTokenHash
	// (compare-and-swap). It reports whether the swap happened; false means
	// the presented token was stale, i.e. a concurrent refresh or a replay.
	Rotate(ctx context.Context, id ids.ID, oldRefreshTokenHash, newRefreshTokenHash string, newExpiresAt time.Time) (rotated bool, err error)
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

// PasswordHasher hashes and verifies passwords (argon2id in production).
type PasswordHasher interface {
	Hash(password string) (string, error)
	Verify(hash, password string) bool
	// NeedsRehash reports whether hash was produced by a weaker algorithm or
	// weaker parameters than the current policy.
	NeedsRehash(hash string) bool
	// VerifyDummy burns the same CPU/memory as a real Verify against a
	// throwaway hash, so unknown-account logins are not distinguishable by
	// response time.
	VerifyDummy(password string)
}

// LoginLimiter throttles credential guessing per account. Allow reports
// whether another attempt for key is permitted right now.
type LoginLimiter interface {
	Allow(ctx context.Context, key string) (bool, error)
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
