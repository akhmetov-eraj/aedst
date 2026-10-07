package application

import (
	"context"
	"crypto/sha256"
	"crypto/subtle"
	"encoding/hex"
	"errors"
	"log/slog"
	"net/mail"
	"strings"
	"time"

	sharederrors "github.com/aedst/server/internal/shared/errors"
	"github.com/aedst/server/internal/shared/ids"
	sharedtime "github.com/aedst/server/internal/shared/time"

	"github.com/aedst/server/internal/modules/auth/domain"
)

// defaultRoles is assigned to every session until real role assignment
// exists (RBAC is out of scope for Phase 1, see docs/content/modules.md).
var defaultRoles = []string{"user"}

// MaxPasswordBytes bounds the password fed to the hasher. A memory-hard hash
// of an unbounded input is a CPU/memory DoS vector.
const MaxPasswordBytes = 128

type Service struct {
	credentials  CredentialRepository
	sessions     SessionRepository
	tokens       TokenIssuer
	hasher       PasswordHasher
	clock        sharedtime.Clock
	loginLimiter LoginLimiter
	logger       *slog.Logger
}

// Option customises optional collaborators of Service.
type Option func(*Service)

// WithLoginLimiter enables per-account throttling of Login.
func WithLoginLimiter(l LoginLimiter) Option { return func(s *Service) { s.loginLimiter = l } }

// WithLogger sets the logger used for security-relevant events (reuse
// detection, limiter failures). Defaults to slog.Default().
func WithLogger(l *slog.Logger) Option { return func(s *Service) { s.logger = l } }

func NewService(credentials CredentialRepository, sessions SessionRepository, tokens TokenIssuer, hasher PasswordHasher, clock sharedtime.Clock, opts ...Option) *Service {
	s := &Service{credentials: credentials, sessions: sessions, tokens: tokens, hasher: hasher, clock: clock, logger: slog.Default()}
	for _, opt := range opts {
		opt(s)
	}
	return s
}

// normalizeEmail makes email lookups case-insensitive: "A@x.com" and
// "a@x.com" are the same account.
func normalizeEmail(email string) string {
	return strings.ToLower(strings.TrimSpace(email))
}

// Register creates a new credential and its first session in one step.
func (s *Service) Register(ctx context.Context, email, password, deviceID string) (User, TokenPair, error) {
	email = normalizeEmail(email)
	if _, err := mail.ParseAddress(email); err != nil {
		return User{}, TokenPair{}, sharederrors.Validation("auth.Register", "invalid email")
	}
	if len(password) > MaxPasswordBytes {
		return User{}, TokenPair{}, sharederrors.Validation("auth.Register", "password too long")
	}

	if _, err := s.credentials.FindByEmail(ctx, email); err == nil {
		return User{}, TokenPair{}, sharederrors.AlreadyExists("auth.Register", "email already registered")
	}

	hash, err := s.hasher.Hash(password)
	if err != nil {
		return User{}, TokenPair{}, sharederrors.Wrap("auth.Register", err)
	}

	cred := domain.Credential{
		UserID:       ids.New(),
		Email:        email,
		PasswordHash: hash,
		CreatedAt:    s.clock.Now(),
	}
	if err := s.credentials.Create(ctx, cred); err != nil {
		return User{}, TokenPair{}, err
	}

	pair, err := s.IssueSession(ctx, cred.UserID, deviceID)
	if err != nil {
		return User{}, TokenPair{}, err
	}

	return toUser(cred), pair, nil
}

// GetCurrentUser looks up the identity behind userID, which callers derive
// from the authenticated request context (platform/auth.FromContext) —
// never from a request field (see api/proto/aedst/auth/v1/auth.proto's
// GetCurrentUser doc comment on why that's an IDOR risk, not a convenience).
func (s *Service) GetCurrentUser(ctx context.Context, userID ids.ID) (User, error) {
	cred, err := s.credentials.FindByUserID(ctx, userID)
	if err != nil {
		return User{}, err
	}
	return toUser(cred), nil
}

func toUser(cred domain.Credential) User {
	return User{ID: cred.UserID, Email: cred.Email, CreatedAt: cred.CreatedAt}
}

// IssueSession creates a new session for userID/deviceID and returns the
// initial token pair. Called by Register right after creating the
// credential, and by Login below.
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

// Login verifies credentials and issues a new session. Unknown accounts and
// wrong passwords are indistinguishable to the caller in both message and
// timing, and attempts are throttled per account.
func (s *Service) Login(ctx context.Context, email, password, deviceID string) (ids.ID, TokenPair, error) {
	email = normalizeEmail(email)
	invalid := func() (ids.ID, TokenPair, error) {
		return ids.Nil, TokenPair{}, sharederrors.Unauthorized("auth.Login", "invalid email or password")
	}

	if s.loginLimiter != nil {
		allowed, err := s.loginLimiter.Allow(ctx, email)
		if err != nil {
			s.logger.WarnContext(ctx, "auth: login limiter unavailable, failing open", slog.Any("error", err))
		} else if !allowed {
			return ids.Nil, TokenPair{}, sharederrors.RateLimited("auth.Login", "too many attempts, try again later")
		}
	}
	if len(password) > MaxPasswordBytes {
		s.hasher.VerifyDummy(password[:MaxPasswordBytes])
		return invalid()
	}

	cred, err := s.credentials.FindByEmail(ctx, email)
	if err != nil {
		s.hasher.VerifyDummy(password)
		return invalid()
	}

	if !s.hasher.Verify(cred.PasswordHash, password) {
		return invalid()
	}

	if s.hasher.NeedsRehash(cred.PasswordHash) {
		s.upgradeHash(ctx, cred.UserID, password)
	}

	pair, err := s.IssueSession(ctx, cred.UserID, deviceID)
	if err != nil {
		return ids.Nil, TokenPair{}, err
	}
	return cred.UserID, pair, nil
}

// upgradeHash re-hashes a just-verified password with the current policy.
// Best effort: a failure must not fail the login that triggered it.
func (s *Service) upgradeHash(ctx context.Context, userID ids.ID, password string) {
	hash, err := s.hasher.Hash(password)
	if err == nil {
		err = s.credentials.UpdatePasswordHash(ctx, userID, hash)
	}
	if err != nil {
		s.logger.WarnContext(ctx, "auth: password hash upgrade failed", slog.Any("error", err))
	}
}

// RefreshToken rotates a session's refresh token. Rotation is an atomic
// compare-and-swap on the stored hash: if the presented token is not the
// session's current one — an already-rotated token being replayed, or two
// concurrent refreshes racing — the whole session is revoked, since that
// indicates token theft/reuse (RFC 9700 §4.14 refresh-token reuse detection).
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

	presentedHash := hashToken(rawRefreshToken)
	if subtle.ConstantTimeCompare([]byte(session.RefreshTokenHash), []byte(presentedHash)) != 1 {
		return TokenPair{}, s.reuseDetected(ctx, session.ID, now)
	}

	pair, err := s.tokens.IssuePair(session.UserID, session.ID, session.DeviceID, defaultRoles)
	if err != nil {
		return TokenPair{}, sharederrors.Wrap("auth.RefreshToken", err)
	}

	rotated, err := s.sessions.Rotate(ctx, session.ID, presentedHash, hashToken(pair.RefreshToken), now.Add(pair.RefreshTTL))
	if err != nil {
		return TokenPair{}, err
	}
	if !rotated {
		// Lost the race against another refresh with the same token.
		return TokenPair{}, s.reuseDetected(ctx, session.ID, now)
	}

	return pair, nil
}

// reuseDetected revokes the whole session (every token derived from it) and
// reports the reuse to the caller as an authentication failure.
func (s *Service) reuseDetected(ctx context.Context, sessionID ids.ID, now time.Time) error {
	if err := s.sessions.Revoke(ctx, sessionID, now); err != nil {
		s.logger.ErrorContext(ctx, "auth: failed to revoke session after refresh-token reuse", slog.String("session_id", sessionID.String()), slog.Any("error", err))
	}
	s.logger.WarnContext(ctx, "auth: refresh token reuse detected, session revoked", slog.String("session_id", sessionID.String()))
	return sharederrors.Unauthorized("auth.RefreshToken", "refresh token reuse detected")
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
