package domain

import (
	"time"

	"github.com/aedst/server/internal/shared/ids"
)

type Profile struct {
	UserID      ids.ID
	DisplayName string
	Bio         string
	AvatarURL   string
	UpdatedAt   time.Time
}
