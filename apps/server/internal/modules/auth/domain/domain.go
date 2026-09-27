// Package domain holds the auth module's entities and domain errors. It
// imports nothing outside the standard library and shared primitives — no
// infrastructure, no other module (see docs/modules.md).
package domain

import (
	"time"

	"github.com/aedst/server/internal/shared/ids"
)

// Credential is the auth module's record of how a user proves identity. It is
// keyed by UserID but auth does not otherwise know anything about that user —
// the users module owns the rest of the identity record.
type Credential struct {
	UserID       ids.ID
	Email        string
	PasswordHash string
	CreatedAt    time.Time
}

// Session represents one device's refresh-token lineage. A new Session is
// created at login/registration; refreshing rotates RefreshTokenHash in
// place; logout sets RevokedAt.
type Session struct {
	ID               ids.ID
	UserID           ids.ID
	DeviceID         string
	RefreshTokenHash string
	ExpiresAt        time.Time
	RevokedAt        *time.Time
	CreatedAt        time.Time
}

func (s Session) IsActive(now time.Time) bool {
	return s.RevokedAt == nil && now.Before(s.ExpiresAt)
}
