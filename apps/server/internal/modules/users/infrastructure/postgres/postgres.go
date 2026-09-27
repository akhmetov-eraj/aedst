// Package postgres implements the users module's Repository against the
// sqlc-generated queries in gen/db/users. Not imported outside this module.
package postgres

import (
	"context"
	"errors"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgconn"
	"github.com/jackc/pgx/v5/pgtype"
	"github.com/jackc/pgx/v5/pgxpool"

	usersdb "github.com/aedst/server/gen/db/users"
	"github.com/aedst/server/internal/modules/users/domain"
	sharederrors "github.com/aedst/server/internal/shared/errors"
	"github.com/aedst/server/internal/shared/ids"
)

const postgresUniqueViolation = "23505"

type Repository struct {
	pool *pgxpool.Pool
}

func NewRepository(pool *pgxpool.Pool) *Repository {
	return &Repository{pool: pool}
}

func (r *Repository) Create(ctx context.Context, user domain.User) error {
	q := usersdb.New(r.pool)
	err := q.CreateUser(ctx, usersdb.CreateUserParams{
		ID:        toPgUUID(user.ID),
		Email:     user.Email,
		Roles:     user.Roles,
		CreatedAt: pgtype.Timestamptz{Time: user.CreatedAt, Valid: true},
	})
	if err != nil {
		if isUniqueViolation(err) {
			return sharederrors.AlreadyExists("users.Create", "email already registered")
		}
		return sharederrors.Wrap("users.Create", err)
	}
	return nil
}

func (r *Repository) FindByID(ctx context.Context, id ids.ID) (domain.User, error) {
	q := usersdb.New(r.pool)
	row, err := q.FindUserByID(ctx, toPgUUID(id))
	if err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return domain.User{}, sharederrors.NotFound("users.FindByID", "user not found")
		}
		return domain.User{}, sharederrors.Wrap("users.FindByID", err)
	}
	return toDomain(row), nil
}

func (r *Repository) FindByEmail(ctx context.Context, email string) (domain.User, error) {
	q := usersdb.New(r.pool)
	row, err := q.FindUserByEmail(ctx, email)
	if err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return domain.User{}, sharederrors.NotFound("users.FindByEmail", "user not found")
		}
		return domain.User{}, sharederrors.Wrap("users.FindByEmail", err)
	}
	return toDomain(row), nil
}

func toDomain(row usersdb.UsersUser) domain.User {
	return domain.User{
		ID:        ids.ID(row.ID.Bytes),
		Email:     row.Email,
		Roles:     row.Roles,
		CreatedAt: row.CreatedAt.Time,
	}
}

func toPgUUID(id ids.ID) pgtype.UUID {
	return pgtype.UUID{Bytes: id, Valid: !id.IsNil()}
}

func isUniqueViolation(err error) bool {
	var pgErr *pgconn.PgError
	return errors.As(err, &pgErr) && pgErr.Code == postgresUniqueViolation
}
