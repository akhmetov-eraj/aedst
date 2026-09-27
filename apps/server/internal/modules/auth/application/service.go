package application

import (
	"context"
	"crypto/sha256"
	"crypto/subtle"
	"encoding/hex"
	"errors"

	sharederrors "github.com/aedst/server/internal/shared/errors"
	"github.com/aedst/server/internal/shared/ids"
	sharedtime "github.com/aedst/server/internal/shared/time"

	"github.com/aedst/server/internal/modules/auth/domain"
)

// defaultRoles is assigned to every session until the users module owns real
// role assignment (see docs/modules.md — RBAC is out of scope for Phase 1).
var defaultRoles = []string{"user"}

type Service struct {
	credentials CredentialRepository
	sessions    SessionRepository
	tokens      TokenIssuer
	hasher      PasswordHasher
	clock       sharedtime.Clock
}

func NewService(credentials CredentialRepository, sessions SessionRepository, tokens TokenIssuer, hasher PasswordHasher, clock sharedtime.Clock) *Service {
	return &Service{credentials: credentials, sessions: sessions, tokens: tokens, hasher: hasher, clock: clock}
}

// CreateCredential stores a new credential for userID. Called by the users
// module's RegisterUser flow in-process (users depends on auth — see
// docs/modules.md) after it has decided the email is available.
func (s *Service) CreateCredential(ctx context.Context, userID ids.ID, email, password string) error {
	hash, err := s.hasher.Hash(password)
	if err != nil {
		return sharederrors.Wrap("auth.CreateCredential", err)
	}

	err = s.credentials.Create(ctx, domain.Credential{
		UserID:       userID,
		Email:        email,
		PasswordHash: hash,
		CreatedAt:    s.clock.Now(),
	})
	if err != nil {
		return err
	}
	return nil
}

// IssueSession creates a new session for userID/deviceID and returns the
// initial token pair. Called by users.RegisterUser right after
// CreateCredential, and by Login below.
func (s *Service) IssueSession(ctx context.Context, userID ids.ID, deviceID string) (TokenPair, error) {
	sessionID := ids.New()

	pair, err := s.tokens.IssuePair(userID, sessionID, deviceID, defaultRoles)
	if err != nil {
		return TokenPair{}, sharederrors.Wrap("auth.IssueSession", err)
	}

	now := s.clock.Now()
	err = s.sessions.Create(ctx, domain.Session{
		ID:               sessionID,
		UserID:           userID,
		DeviceID:         deviceID,
		RefreshTokenHash: hashToken(pair.RefreshToken),
		ExpiresAt:        now.Add(pair.RefreshTTL),
		CreatedAt:        now,
	})
	if err != nil {
		return TokenPair{}, err
	}

	return pair, nil
}

// Login verifies credentials and issues a new session.
func (s *Service) Login(ctx context.Context, email, password, deviceID string) (ids.ID, TokenPair, error) {
	cred, err := s.credentials.FindByEmail(ctx, email)
	if err != nil {
		return ids.Nil, TokenPair{}, sharederrors.Unauthorized("auth.Login", "invalid email or password")
	}

	if !s.hasher.Verify(cred.PasswordHash, password) {
		return ids.Nil, TokenPair{}, sharederrors.Unauthorized("auth.Login", "invalid email or password")
	}

	pair, err := s.IssueSession(ctx, cred.UserID, deviceID)
	if err != nil {
		return ids.Nil, TokenPair{}, err
	}
	return cred.UserID, pair, nil
}

// RefreshToken rotates a session's refresh token. If the presented token
// doesn't match the session's current stored hash — i.e. an already-rotated
// (stale) token is being replayed — the session is revoked outright, since
// that indicates token theft/reuse.
func (s *Service) RefreshToken(ctx context.Context, rawRefreshToken string) (TokenPair, error) {
	claims, err := s.tokens.VerifyRefreshToken(ctx, rawRefreshToken)
	if err != nil {
		return TokenPair{}, sharederrors.Unauthorized("auth.RefreshToken", "invalid refresh token")
	}

	session, err := s.sessions.FindByID(ctx, claims.SessionID)
	if err != nil {
		return TokenPair{}, sharederrors.Unauthorized("auth.RefreshToken", "session not found")
	}

	now := s.clock.Now()
	if !session.IsActive(now) {
		return TokenPair{}, sharederrors.Unauthorized("auth.RefreshToken", "session expired or revoked")
	}

	if subtle.ConstantTimeCompare([]byte(session.RefreshTokenHash), []byte(hashToken(rawRefreshToken))) != 1 {
		_ = s.sessions.Revoke(ctx, session.ID, now)
		return TokenPair{}, sharederrors.Unauthorized("auth.RefreshToken", "refresh token reuse detected")
	}

	pair, err := s.tokens.IssuePair(session.UserID, session.ID, session.DeviceID, defaultRoles)
	if err != nil {
		return TokenPair{}, sharederrors.Wrap("auth.RefreshToken", err)
	}

	if err := s.sessions.Rotate(ctx, session.ID, hashToken(pair.RefreshToken), now.Add(pair.RefreshTTL)); err != nil {
		return TokenPair{}, err
	}

	return pair, nil
}

// Logout revokes the session backing rawRefreshToken.
func (s *Service) Logout(ctx context.Context, rawRefreshToken string) error {
	claims, err := s.tokens.VerifyRefreshToken(ctx, rawRefreshToken)
	if err != nil {
		// An already-invalid token has nothing to revoke; logout is
		// idempotent from the caller's point of view.
		return nil
	}
	return s.sessions.Revoke(ctx, claims.SessionID, s.clock.Now())
}

func hashToken(raw string) string {
	sum := sha256.Sum256([]byte(raw))
	return hex.EncodeToString(sum[:])
}

// ErrCredentialExists is returned by CredentialRepository.Create when the
// email is already registered.
var ErrCredentialExists = errors.New("auth: credential already exists")
