package domain

import (
	"time"

	"github.com/aedst/server/internal/shared/ids"
)

// User is the core identity record. Other modules never read this directly —
// they either call users/api or consume UserRegistered (see docs/events.md).
type User struct {
	ID        ids.ID
	Email     string
	Roles     []string
	CreatedAt time.Time
}
