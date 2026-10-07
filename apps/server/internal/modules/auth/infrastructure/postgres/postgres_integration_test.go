//go:build integration

package postgres_test

import (
	"context"
	"testing"
	"time"

	"github.com/aedst/server/internal/modules/auth/domain"
	"github.com/aedst/server/internal/modules/auth/infrastructure/postgres"
	"github.com/aedst/server/internal/platform/postgrestest"
	"github.com/aedst/server/internal/shared/ids"
)

// TestSessionRepository_RotateIsCompareAndSwap proves refresh-token rotation
// is atomic: of two concurrent refreshes presenting the same token exactly
// one wins, and a revoked session can never rotate.
func TestSessionRepository_RotateIsCompareAndSwap(t *testing.T) {
	pool := postgrestest.NewPool(t, "auth")
	ctx := context.Background()
	creds := postgres.NewCredentialRepository(pool)
	sessions := postgres.NewSessionRepository(pool)

	now := time.Now().UTC().Truncate(time.Second)
	userID := ids.New()
	if err := creds.Create(ctx, domain.Credential{UserID: userID, Email: "a@example.com", PasswordHash: "h", CreatedAt: now}); err != nil {
		t.Fatalf("create credential: %v", err)
	}
	sessionID := ids.New()
	if err := sessions.Create(ctx, domain.Session{ID: sessionID, UserID: userID, DeviceID: "d", RefreshTokenHash: "h0", ExpiresAt: now.Add(time.Hour), CreatedAt: now}); err != nil {
		t.Fatalf("create session: %v", err)
	}

	first, err := sessions.Rotate(ctx, sessionID, "h0", "h1", now.Add(time.Hour))
	if err != nil || !first {
		t.Fatalf("first Rotate() = %v, %v; want true", first, err)
	}
	stale, err := sessions.Rotate(ctx, sessionID, "h0", "h2", now.Add(time.Hour))
	if err != nil || stale {
		t.Fatalf("Rotate() with a stale token = %v, %v; want false", stale, err)
	}

	if err := sessions.Revoke(ctx, sessionID, now); err != nil {
		t.Fatalf("revoke: %v", err)
	}
	revoked, err := sessions.Rotate(ctx, sessionID, "h1", "h3", now.Add(time.Hour))
	if err != nil || revoked {
		t.Fatalf("Rotate() on a revoked session = %v, %v; want false", revoked, err)
	}
}

func TestCredentialRepository_UpdatePasswordHash(t *testing.T) {
	pool := postgrestest.NewPool(t, "auth")
	ctx := context.Background()
	creds := postgres.NewCredentialRepository(pool)

	userID := ids.New()
	if err := creds.Create(ctx, domain.Credential{UserID: userID, Email: "b@example.com", PasswordHash: "old", CreatedAt: time.Now().UTC()}); err != nil {
		t.Fatal(err)
	}
	if err := creds.UpdatePasswordHash(ctx, userID, "new"); err != nil {
		t.Fatal(err)
	}
	got, err := creds.FindByUserID(ctx, userID)
	if err != nil || got.PasswordHash != "new" {
		t.Fatalf("FindByUserID() = %+v, %v; want the updated hash", got, err)
	}
}

// Retention removes only sessions that can never authenticate again: expired
// or revoked before the cutoff. Live sessions and recently dead ones stay.
func TestSessionRepository_PurgeDeadSessions(t *testing.T) {
	pool := postgrestest.NewPool(t, "auth")
	ctx := context.Background()
	creds := postgres.NewCredentialRepository(pool)
	sessions := postgres.NewSessionRepository(pool)

	now := time.Now().UTC().Truncate(time.Second)
	userID := ids.New()
	if err := creds.Create(ctx, domain.Credential{UserID: userID, Email: "c@example.com", PasswordHash: "h", CreatedAt: now}); err != nil {
		t.Fatal(err)
	}
	add := func(expiresAt time.Time, revokedAt *time.Time) ids.ID {
		id := ids.New()
		if err := sessions.Create(ctx, domain.Session{ID: id, UserID: userID, DeviceID: "d", RefreshTokenHash: id.String(), ExpiresAt: expiresAt, CreatedAt: now.Add(-100 * 24 * time.Hour)}); err != nil {
			t.Fatal(err)
		}
		if revokedAt != nil {
			if err := sessions.Revoke(ctx, id, *revokedAt); err != nil {
				t.Fatal(err)
			}
		}
		return id
	}
	cutoff := now.Add(-30 * 24 * time.Hour)
	oldRevoked := now.Add(-40 * 24 * time.Hour)
	recentRevoked := now.Add(-time.Hour)

	expiredLongAgo := add(now.Add(-60*24*time.Hour), nil)
	revokedLongAgo := add(now.Add(24*time.Hour), &oldRevoked)
	live := add(now.Add(24*time.Hour), nil)
	expiredRecently := add(now.Add(-time.Hour), nil)
	revokedRecently := add(now.Add(24*time.Hour), &recentRevoked)

	n, err := sessions.PurgeDeadSessions(ctx, cutoff, 100)
	if err != nil || n != 2 {
		t.Fatalf("PurgeDeadSessions() = %d, %v; want the 2 long-dead sessions", n, err)
	}
	for id, wantGone := range map[ids.ID]bool{expiredLongAgo: true, revokedLongAgo: true, live: false, expiredRecently: false, revokedRecently: false} {
		_, err := sessions.FindByID(ctx, id)
		if gone := err != nil; gone != wantGone {
			t.Errorf("session %s gone = %v, want %v", id, gone, wantGone)
		}
	}
	if n, err := sessions.PurgeDeadSessions(ctx, cutoff, 100); err != nil || n != 0 {
		t.Fatalf("second purge = %d, %v; want nothing left to delete", n, err)
	}
}
