// Package postgres implements the auth module's repositories against the
// sqlc-generated queries in gen/db/auth. Nothing outside this module imports
// this package directly (see docs/architecture.md) — other modules go through
// modules/auth/api.
package postgres

import (
	"context"
	"errors"
	"time"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgconn"
	"github.com/jackc/pgx/v5/pgtype"
	"github.com/jackc/pgx/v5/pgxpool"

	authdb "github.com/aedst/server/gen/db/auth"
	"github.com/aedst/server/internal/modules/auth/domain"
	sharederrors "github.com/aedst/server/internal/shared/errors"
	"github.com/aedst/server/internal/shared/ids"
)

type CredentialRepository struct {
	pool *pgxpool.Pool
}

func NewCredentialRepository(pool *pgxpool.Pool) *CredentialRepository {
	return &CredentialRepository{pool: pool}
}

func (r *CredentialRepository) Create(ctx context.Context, cred domain.Credential) error {
	q := authdb.New(r.pool)
	err := q.CreateCredential(ctx, authdb.CreateCredentialParams{
		UserID:       toPgUUID(cred.UserID),
		Email:        cred.Email,
		PasswordHash: cred.PasswordHash,
		CreatedAt:    toPgTimestamptz(cred.CreatedAt),
	})
	if err != nil {
		if isUniqueViolation(err) {
			return sharederrors.AlreadyExists("auth.CreateCredential", "email already registered")
		}
		return sharederrors.Wrap("auth.CreateCredential", err)
	}
	return nil
}

func (r *CredentialRepository) FindByEmail(ctx context.Context, email string) (domain.Credential, error) {
	q := authdb.New(r.pool)
	row, err := q.FindCredentialByEmail(ctx, email)
	if err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return domain.Credential{}, sharederrors.NotFound("auth.FindCredentialByEmail", "credential not found")
		}
		return domain.Credential{}, sharederrors.Wrap("auth.FindCredentialByEmail", err)
	}
	return domain.Credential{
		UserID:       fromPgUUID(row.UserID),
		Email:        row.Email,
		PasswordHash: row.PasswordHash,
		CreatedAt:    row.CreatedAt.Time,
	}, nil
}

type SessionRepository struct {
	pool *pgxpool.Pool
}

func NewSessionRepository(pool *pgxpool.Pool) *SessionRepository {
	return &SessionRepository{pool: pool}
}

func (r *SessionRepository) Create(ctx context.Context, session domain.Session) error {
	q := authdb.New(r.pool)
	err := q.CreateSession(ctx, authdb.CreateSessionParams{
		ID:               toPgUUID(session.ID),
		UserID:           toPgUUID(session.UserID),
		DeviceID:         session.DeviceID,
		RefreshTokenHash: session.RefreshTokenHash,
		ExpiresAt:        toPgTimestamptz(session.ExpiresAt),
		CreatedAt:        toPgTimestamptz(session.CreatedAt),
	})
	if err != nil {
		return sharederrors.Wrap("auth.CreateSession", err)
	}
	return nil
}

func (r *SessionRepository) FindByID(ctx context.Context, id ids.ID) (domain.Session, error) {
	q := authdb.New(r.pool)
	row, err := q.FindSessionByID(ctx, toPgUUID(id))
	if err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return domain.Session{}, sharederrors.NotFound("auth.FindSessionByID", "session not found")
		}
		return domain.Session{}, sharederrors.Wrap("auth.FindSessionByID", err)
	}

	var revokedAt *time.Time
	if row.RevokedAt.Valid {
		t := row.RevokedAt.Time
		revokedAt = &t
	}

	return domain.Session{
		ID:               fromPgUUID(row.ID),
		UserID:           fromPgUUID(row.UserID),
		DeviceID:         row.DeviceID,
		RefreshTokenHash: row.RefreshTokenHash,
		ExpiresAt:        row.ExpiresAt.Time,
		RevokedAt:        revokedAt,
		CreatedAt:        row.CreatedAt.Time,
	}, nil
}

func (r *SessionRepository) Rotate(ctx context.Context, id ids.ID, newRefreshTokenHash string, newExpiresAt time.Time) error {
	q := authdb.New(r.pool)
	err := q.RotateSession(ctx, authdb.RotateSessionParams{
		ID:               toPgUUID(id),
		RefreshTokenHash: newRefreshTokenHash,
		ExpiresAt:        toPgTimestamptz(newExpiresAt),
	})
	if err != nil {
		return sharederrors.Wrap("auth.RotateSession", err)
	}
	return nil
}

func (r *SessionRepository) Revoke(ctx context.Context, id ids.ID, revokedAt time.Time) error {
	q := authdb.New(r.pool)
	err := q.RevokeSession(ctx, authdb.RevokeSessionParams{
		ID:        toPgUUID(id),
		RevokedAt: toPgTimestamptz(revokedAt),
	})
	if err != nil {
		return sharederrors.Wrap("auth.RevokeSession", err)
	}
	return nil
}

func toPgUUID(id ids.ID) pgtype.UUID {
	return pgtype.UUID{Bytes: id, Valid: !id.IsNil()}
}

func fromPgUUID(u pgtype.UUID) ids.ID {
	return ids.ID(u.Bytes)
}

func toPgTimestamptz(t time.Time) pgtype.Timestamptz {
	return pgtype.Timestamptz{Time: t, Valid: !t.IsZero()}
}

// postgresUniqueViolation is the SQLSTATE code for a unique constraint
// violation: https://www.postgresql.org/docs/current/errcodes-appendix.html
const postgresUniqueViolation = "23505"

func isUniqueViolation(err error) bool {
	var pgErr *pgconn.PgError
	return errors.As(err, &pgErr) && pgErr.Code == postgresUniqueViolation
}
