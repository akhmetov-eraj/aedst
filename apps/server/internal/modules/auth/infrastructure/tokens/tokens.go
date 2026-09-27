// Package tokens adapts platform/auth.TokenService to the auth module's
// application.TokenIssuer interface, so application never imports the
// jwx-specific platform package directly.
package tokens

import (
	"context"

	"github.com/aedst/server/internal/modules/auth/application"
	platformauth "github.com/aedst/server/internal/platform/auth"
	"github.com/aedst/server/internal/shared/ids"
)

type Adapter struct {
	service *platformauth.TokenService
}

func NewAdapter(service *platformauth.TokenService) *Adapter {
	return &Adapter{service: service}
}

func (a *Adapter) IssuePair(userID, sessionID ids.ID, deviceID string, roles []string) (application.TokenPair, error) {
	pair, err := a.service.IssuePair(userID, sessionID, deviceID, roles)
	if err != nil {
		return application.TokenPair{}, err
	}
	return application.TokenPair{
		AccessToken:  pair.AccessToken,
		RefreshToken: pair.RefreshToken,
		AccessTTL:    pair.AccessTTL,
		RefreshTTL:   pair.RefreshTTL,
	}, nil
}

func (a *Adapter) VerifyRefreshToken(ctx context.Context, raw string) (application.RefreshClaims, error) {
	claims, err := a.service.VerifyRefreshToken(ctx, raw)
	if err != nil {
		return application.RefreshClaims{}, err
	}
	return application.RefreshClaims{UserID: claims.UserID, SessionID: claims.SessionID}, nil
}
