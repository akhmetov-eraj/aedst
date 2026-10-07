package application

import (
	"context"
	"time"

	"github.com/aedst/server/internal/shared/ids"
	"github.com/aedst/server/internal/shared/pagination"
)

// DirectorySortToken binds directory cursors to their ordering.
const DirectorySortToken = "auth:created_at:desc"

// DirectoryUser is one first-party account as an operator sees it (never the
// password hash).
type DirectoryUser struct {
	ID        ids.ID
	Email     string
	CreatedAt time.Time
}

// UserPage is one keyset page of the directory.
type UserPage struct {
	Users []DirectoryUser
	Next  *pagination.Cursor
}

// DirectoryRepository lists first-party accounts.
type DirectoryRepository interface {
	ListUsers(ctx context.Context, search string, limit int, after *pagination.Cursor) (UserPage, error)
	CountUsers(ctx context.Context) (int64, error)
}

// Directory serves auth/api.Directory. It exists only when first-party auth
// is enabled: with an external IdP the IdP is the user directory.
type Directory struct {
	repo DirectoryRepository
}

func NewDirectory(repo DirectoryRepository) *Directory { return &Directory{repo: repo} }

func (d *Directory) ListUsers(ctx context.Context, search string, limit int32, after *pagination.Cursor) (UserPage, error) {
	return d.repo.ListUsers(ctx, search, int(pagination.ClampLimit(limit)), after)
}

func (d *Directory) CountUsers(ctx context.Context) (int64, error) { return d.repo.CountUsers(ctx) }
