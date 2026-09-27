// Package api is the profile module's public boundary (see docs/modules.md).
package api

import (
	"context"

	"github.com/aedst/server/internal/modules/profile/application"
	"github.com/aedst/server/internal/modules/profile/domain"
	"github.com/aedst/server/internal/shared/ids"
)

type Profile = domain.Profile

type Service interface {
	UpdateProfile(ctx context.Context, userID ids.ID, displayName, bio, avatarURL string) (Profile, error)
	GetProfile(ctx context.Context, userID ids.ID) (Profile, error)
}

var _ Service = (*application.Service)(nil)
