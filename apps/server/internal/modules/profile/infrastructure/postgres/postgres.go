// Package postgres implements the profile module's Repository against the
// sqlc-generated queries in gen/db/profile. Not imported outside this module.
package postgres

import (
	"context"
	"errors"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgtype"
	"github.com/jackc/pgx/v5/pgxpool"

	profiledb "github.com/aedst/server/gen/db/profile"
	"github.com/aedst/server/internal/modules/profile/domain"
	sharederrors "github.com/aedst/server/internal/shared/errors"
	"github.com/aedst/server/internal/shared/ids"
)

type Repository struct {
	pool *pgxpool.Pool
}

func NewRepository(pool *pgxpool.Pool) *Repository {
	return &Repository{pool: pool}
}

func (r *Repository) Upsert(ctx context.Context, profile domain.Profile) (domain.Profile, error) {
	q := profiledb.New(r.pool)
	row, err := q.UpsertProfile(ctx, profiledb.UpsertProfileParams{
		UserID:      toPgUUID(profile.UserID),
		DisplayName: profile.DisplayName,
		Bio:         profile.Bio,
		AvatarUrl:   profile.AvatarURL,
		UpdatedAt:   pgtype.Timestamptz{Time: profile.UpdatedAt, Valid: true},
	})
	if err != nil {
		return domain.Profile{}, sharederrors.Wrap("profile.Upsert", err)
	}
	return toDomain(row), nil
}

func (r *Repository) FindByUserID(ctx context.Context, userID ids.ID) (domain.Profile, error) {
	q := profiledb.New(r.pool)
	row, err := q.FindProfileByUserID(ctx, toPgUUID(userID))
	if err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return domain.Profile{}, sharederrors.NotFound("profile.FindByUserID", "profile not found")
		}
		return domain.Profile{}, sharederrors.Wrap("profile.FindByUserID", err)
	}
	return toDomain(row), nil
}

func toDomain(row profiledb.ProfileProfile) domain.Profile {
	return domain.Profile{
		UserID:      ids.ID(row.UserID.Bytes),
		DisplayName: row.DisplayName,
		Bio:         row.Bio,
		AvatarURL:   row.AvatarUrl,
		UpdatedAt:   row.UpdatedAt.Time,
	}
}

func toPgUUID(id ids.ID) pgtype.UUID {
	return pgtype.UUID{Bytes: id, Valid: !id.IsNil()}
}
