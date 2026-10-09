package postgres

import (
	"context"
	"strings"

	"github.com/jackc/pgx/v5/pgtype"

	authdb "github.com/aedst/server/gen/db/auth"
	"github.com/aedst/server/internal/modules/auth/application"
	sharederrors "github.com/aedst/server/internal/shared/errors"
	"github.com/aedst/server/internal/shared/like"
	"github.com/aedst/server/internal/shared/pagination"
)

// ListUsers pages the first-party user directory for operators, newest first.
func (r *CredentialRepository) ListUsers(ctx context.Context, search string, limit int, after *pagination.Cursor) (application.UserPage, error) {
	cursorKey := pgtype.Timestamptz{InfinityModifier: pgtype.Infinity, Valid: true}
	cursorID := pgtype.UUID{Bytes: [16]byte{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, Valid: true}
	if after != nil {
		cursorKey = toPgTimestamptz(after.Key)
		cursorID = toPgUUID(after.ID)
	}
	rows, err := authdb.New(r.pool).ListCredentialsPage(ctx, authdb.ListCredentialsPageParams{
		Search: like.Escape(strings.TrimSpace(search)), CursorKey: cursorKey, CursorID: cursorID, PageLimit: int32(limit) + 1,
	})
	if err != nil {
		return application.UserPage{}, sharederrors.Wrap("auth.ListUsers", err)
	}
	users := make([]application.DirectoryUser, len(rows))
	for i, row := range rows {
		users[i] = application.DirectoryUser{ID: fromPgUUID(row.UserID), Email: row.Email, CreatedAt: row.CreatedAt.Time}
	}
	page := pagination.NewPage(users, limit, func(u application.DirectoryUser) pagination.Cursor {
		return pagination.Cursor{Sort: application.DirectorySortToken, Key: u.CreatedAt, ID: u.ID}
	})
	return application.UserPage{Users: page.Items, Next: page.Next}, nil
}

func (r *CredentialRepository) CountUsers(ctx context.Context) (int64, error) {
	n, err := authdb.New(r.pool).CountCredentials(ctx)
	if err != nil {
		return 0, sharederrors.Wrap("auth.CountUsers", err)
	}
	return n, nil
}
