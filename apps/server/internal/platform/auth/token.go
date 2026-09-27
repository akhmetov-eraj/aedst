// Package auth is the platform's central authentication boundary: it issues
// and verifies access/refresh tokens with jwx, and supports zero-downtime key
// rotation by verifying against a current and a previous signing key. No
// individual module implements its own token logic (see project brief
// section 17) — the auth module's application layer calls this package to
// issue tokens and the httpserver interceptor calls it to verify them.
package auth

import (
	"context"
	"fmt"
	"time"

	"github.com/lestrrat-go/jwx/v4/jwa"
	"github.com/lestrrat-go/jwx/v4/jwk"
	"github.com/lestrrat-go/jwx/v4/jwt"

	"github.com/aedst/server/internal/shared/ids"
)

const (
	claimSessionID = "sid"
	claimDeviceID  = "did"
	claimRoles     = "roles"
	claimTokenUse  = "use"

	tokenUseAccess  = "access"
	tokenUseRefresh = "refresh"
)

// TokenPair is the access/refresh token pair issued at login or refresh.
type TokenPair struct {
	AccessToken  string
	RefreshToken string
	AccessTTL    time.Duration
	RefreshTTL   time.Duration
}

// Config configures token issuing/verification.
type Config struct {
	Issuer          string
	SigningKey      string // current key, used for signing and verification
	PreviousKey     string // optional, verification only, for rotation
	AccessTokenTTL  time.Duration
	RefreshTokenTTL time.Duration
}

// TokenService issues and verifies access/refresh tokens.
type TokenService struct {
	issuer     string
	signingKey jwk.SymmetricKey
	verifyKeys []jwk.SymmetricKey
	accessTTL  time.Duration
	refreshTTL time.Duration
}

func importSymmetricKey(secret string) (jwk.SymmetricKey, error) {
	key, err := jwk.Import[jwk.SymmetricKey]([]byte(secret))
	if err != nil {
		return nil, err
	}
	if err := key.Set(jwk.AlgorithmKey, jwa.HS256()); err != nil {
		return nil, fmt.Errorf("set signing algorithm: %w", err)
	}
	return key, nil
}

func NewTokenService(cfg Config) (*TokenService, error) {
	signingKey, err := importSymmetricKey(cfg.SigningKey)
	if err != nil {
		return nil, fmt.Errorf("auth: import signing key: %w", err)
	}

	verifyKeys := []jwk.SymmetricKey{signingKey}
	if cfg.PreviousKey != "" {
		prevKey, err := importSymmetricKey(cfg.PreviousKey)
		if err != nil {
			return nil, fmt.Errorf("auth: import previous key: %w", err)
		}
		verifyKeys = append(verifyKeys, prevKey)
	}

	return &TokenService{
		issuer:     cfg.Issuer,
		signingKey: signingKey,
		verifyKeys: verifyKeys,
		accessTTL:  cfg.AccessTokenTTL,
		refreshTTL: cfg.RefreshTokenTTL,
	}, nil
}

// IssuePair issues a new access+refresh token pair for the given session.
func (s *TokenService) IssuePair(userID ids.ID, sessionID ids.ID, deviceID string, roles []string) (TokenPair, error) {
	now := time.Now().UTC()

	access, err := s.build(userID, sessionID, deviceID, roles, tokenUseAccess, now, s.accessTTL)
	if err != nil {
		return TokenPair{}, err
	}
	refresh, err := s.build(userID, sessionID, deviceID, roles, tokenUseRefresh, now, s.refreshTTL)
	if err != nil {
		return TokenPair{}, err
	}

	return TokenPair{
		AccessToken:  access,
		RefreshToken: refresh,
		AccessTTL:    s.accessTTL,
		RefreshTTL:   s.refreshTTL,
	}, nil
}

func (s *TokenService) build(userID, sessionID ids.ID, deviceID string, roles []string, use string, now time.Time, ttl time.Duration) (string, error) {
	builder := jwt.NewBuilder().
		Issuer(s.issuer).
		Subject(userID.String()).
		IssuedAt(now).
		Expiration(now.Add(ttl)).
		Claim(claimSessionID, sessionID.String()).
		Claim(claimDeviceID, deviceID).
		Claim(claimTokenUse, use).
		Claim(claimRoles, roles)

	token, err := builder.Build()
	if err != nil {
		return "", fmt.Errorf("auth: build token: %w", err)
	}

	signed, err := jwt.Sign(token, jwt.WithKey(jwa.HS256(), s.signingKey))
	if err != nil {
		return "", fmt.Errorf("auth: sign token: %w", err)
	}

	return string(signed), nil
}

// Claims is the verified, decoded form of a token.
type Claims struct {
	UserID    ids.ID
	SessionID ids.ID
	DeviceID  string
	Roles     []string
	TokenUse  string
}

// VerifyAccessToken verifies and decodes an access token, rejecting refresh
// tokens presented where an access token is expected.
func (s *TokenService) VerifyAccessToken(ctx context.Context, raw string) (Claims, error) {
	claims, err := s.verify(ctx, raw)
	if err != nil {
		return Claims{}, err
	}
	if claims.TokenUse != tokenUseAccess {
		return Claims{}, fmt.Errorf("auth: expected access token, got %q", claims.TokenUse)
	}
	return claims, nil
}

// VerifyRefreshToken verifies and decodes a refresh token.
func (s *TokenService) VerifyRefreshToken(ctx context.Context, raw string) (Claims, error) {
	claims, err := s.verify(ctx, raw)
	if err != nil {
		return Claims{}, err
	}
	if claims.TokenUse != tokenUseRefresh {
		return Claims{}, fmt.Errorf("auth: expected refresh token, got %q", claims.TokenUse)
	}
	return claims, nil
}

func (s *TokenService) verify(ctx context.Context, raw string) (Claims, error) {
	var lastErr error
	for _, key := range s.verifyKeys {
		token, err := jwt.Parse([]byte(raw),
			jwt.WithKey(jwa.HS256(), key),
			jwt.WithValidate(true),
			jwt.WithIssuer(s.issuer),
			jwt.WithContext(ctx),
		)
		if err != nil {
			lastErr = err
			continue
		}
		return claimsFromToken(token)
	}
	return Claims{}, fmt.Errorf("auth: verify token: %w", lastErr)
}

func claimsFromToken(token jwt.Token) (Claims, error) {
	subject, _ := token.Subject()
	userID, err := ids.Parse(subject)
	if err != nil {
		return Claims{}, fmt.Errorf("auth: invalid subject claim: %w", err)
	}

	sessionID, err := ids.Parse(stringField(token, claimSessionID))
	if err != nil {
		return Claims{}, fmt.Errorf("auth: invalid session claim: %w", err)
	}

	return Claims{
		UserID:    userID,
		SessionID: sessionID,
		DeviceID:  stringField(token, claimDeviceID),
		Roles:     stringSliceField(token, claimRoles),
		TokenUse:  stringField(token, claimTokenUse),
	}, nil
}

// stringField and stringSliceField read a custom claim without relying on
// jwt.Get[T]'s exact type assertion, since JSON-decoded claim values arrive as
// any (a []string claim decodes to []any, not []string).
func stringField(token jwt.Token, key string) string {
	v, ok := token.Field(key)
	if !ok {
		return ""
	}
	s, _ := v.(string)
	return s
}

func stringSliceField(token jwt.Token, key string) []string {
	v, ok := token.Field(key)
	if !ok {
		return nil
	}
	switch vals := v.(type) {
	case []string:
		return vals
	case []any:
		out := make([]string, 0, len(vals))
		for _, item := range vals {
			if s, ok := item.(string); ok {
				out = append(out, s)
			}
		}
		return out
	default:
		return nil
	}
}
