package auth

import (
	"context"

	"github.com/aedst/server/internal/shared/ids"
)

// Identity is the authenticated principal, extracted from a verified access
// token by the httpserver auth interceptor and placed on the context. Modules
// consume it via FromContext — they never verify tokens themselves (see
// docs/architecture.md and section 17 of the project brief).
type Identity struct {
	UserID    ids.ID
	SessionID ids.ID
	DeviceID  string
	Roles     []string
}

func (i Identity) HasRole(role string) bool {
	for _, r := range i.Roles {
		if r == role {
			return true
		}
	}
	return false
}

type identityKey struct{}

// WithIdentity returns a context carrying the authenticated identity.
func WithIdentity(ctx context.Context, id Identity) context.Context {
	return context.WithValue(ctx, identityKey{}, id)
}

// FromContext returns the authenticated identity, if any request-scoped
// context carries one.
func FromContext(ctx context.Context) (Identity, bool) {
	id, ok := ctx.Value(identityKey{}).(Identity)
	return id, ok
}
