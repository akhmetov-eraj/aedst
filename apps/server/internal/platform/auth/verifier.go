package auth

import (
	"context"
	"errors"
	"fmt"
	"strings"

	"github.com/lestrrat-go/jwx/v4/jwt"
)

// Verifier verifies a bearer access token and returns its claims. It is the
// only thing the httpserver interceptor knows about authentication, so the
// identity back-end (first-party tokens, an external OIDC provider, or both)
// is a wiring decision in cmd/server, not a code change.
type Verifier interface {
	VerifyAccessToken(ctx context.Context, raw string) (Claims, error)
}

var (
	_ Verifier = (*TokenService)(nil)
	_ Verifier = (*OIDCVerifier)(nil)
	_ Verifier = (*MultiVerifier)(nil)
)

// MultiVerifier routes a token to the verifier responsible for its issuer.
// Routing on the (unverified) iss claim only selects WHICH verifier runs;
// the chosen verifier still fully validates signature, issuer and audience,
// so a forged iss cannot bypass anything.
type MultiVerifier struct {
	firstParty *TokenService
	oidc       *OIDCVerifier
}

// NewMultiVerifier accepts either back-end being nil (disabled), but not both.
func NewMultiVerifier(firstParty *TokenService, oidc *OIDCVerifier) (*MultiVerifier, error) {
	if firstParty == nil && oidc == nil {
		return nil, errors.New("auth: no token verifier configured")
	}
	return &MultiVerifier{firstParty: firstParty, oidc: oidc}, nil
}

// ErrOpaqueToken is returned for a bearer token that is not a JWT. This server
// verifies JWT access tokens offline (no introspection); an IdP issuing opaque
// (reference) tokens must be switched to JWT access tokens (ZITADEL: app
// accessTokenType OIDC_TOKEN_TYPE_JWT, machine user ACCESS_TOKEN_TYPE_JWT).
var ErrOpaqueToken = errors.New("auth: bearer token is not a JWT (opaque access token?): configure the identity provider to issue JWT access tokens")

// looksOpaque reports whether raw cannot be a compact JWS (three dot-separated
// segments), which is how opaque access tokens look.
func looksOpaque(raw string) bool { return strings.Count(raw, ".") != 2 }

func (m *MultiVerifier) VerifyAccessToken(ctx context.Context, raw string) (Claims, error) {
	if looksOpaque(raw) {
		return Claims{}, ErrOpaqueToken
	}
	token, err := jwt.ParseInsecure([]byte(raw))
	if err != nil {
		return Claims{}, fmt.Errorf("auth: malformed token: %w", err)
	}
	issuer, _ := token.Issuer()

	switch {
	case m.oidc != nil && issuer == m.oidc.Issuer():
		return m.oidc.VerifyAccessToken(ctx, raw)
	case m.firstParty != nil && issuer == m.firstParty.issuer:
		return m.firstParty.VerifyAccessToken(ctx, raw)
	default:
		return Claims{}, errors.New("auth: unknown token issuer")
	}
}
