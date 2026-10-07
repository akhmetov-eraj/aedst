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

func (r *CredentialRepository) FindByUserID(ctx context.Context, userID ids.ID) (domain.Credential, error) {
	q := authdb.New(r.pool)
	row, err := q.FindCredentialByUserID(ctx, toPgUUID(userID))
	if err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return domain.Credential{}, sharederrors.NotFound("auth.FindCredentialByUserID", "credential not found")
		}
		return domain.Credential{}, sharederrors.Wrap("auth.FindCredentialByUserID", err)
	}
	return domain.Credential{
		UserID:       fromPgUUID(row.UserID),
		Email:        row.Email,
		PasswordHash: row.PasswordHash,
		CreatedAt:    row.CreatedAt.Time,
	}, nil
}

func (r *CredentialRepository) UpdatePasswordHash(ctx context.Context, userID ids.ID, hash string) error {
	q := authdb.New(r.pool)
	if err := q.UpdateCredentialPasswordHash(ctx, authdb.UpdateCredentialPasswordHashParams{UserID: toPgUUID(userID), PasswordHash: hash}); err != nil {
		return sharederrors.Wrap("auth.UpdatePasswordHash", err)
	}
	return nil
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

func (r *SessionRepository) Rotate(ctx context.Context, id ids.ID, oldRefreshTokenHash, newRefreshTokenHash string, newExpiresAt time.Time) (bool, error) {
	q := authdb.New(r.pool)
	affected, err := q.RotateSession(ctx, authdb.RotateSessionParams{
		ID:                  toPgUUID(id),
		OldRefreshTokenHash: oldRefreshTokenHash,
		NewRefreshTokenHash: newRefreshTokenHash,
		ExpiresAt:           toPgTimestamptz(newExpiresAt),
	})
	if err != nil {
		return false, sharederrors.Wrap("auth.RotateSession", err)
	}
	return affected == 1, nil
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

// PurgeDeadSessions deletes up to limit sessions that expired or were revoked
// before cutoff (retention: see internal/platform/retention) and returns how
// many it deleted.
func (r *SessionRepository) PurgeDeadSessions(ctx context.Context, cutoff time.Time, limit int) (int64, error) {
	n, err := authdb.New(r.pool).PurgeSessions(ctx, authdb.PurgeSessionsParams{
		Cutoff:     toPgTimestamptz(cutoff),
		PurgeLimit: int32(limit),
	})
	if err != nil {
		return 0, sharederrors.Wrap("auth.PurgeDeadSessions", err)
	}
	return n, nil
}
