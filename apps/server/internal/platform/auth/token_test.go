package auth_test

import (
	"context"
	"testing"
	"time"

	"github.com/aedst/server/internal/platform/auth"
	"github.com/aedst/server/internal/shared/ids"
)

func newTestService(t *testing.T, signingKey, previousKey string) *auth.TokenService {
	t.Helper()
	svc, err := auth.NewTokenService(auth.Config{
		Issuer:          "test-issuer",
		SigningKey:      signingKey,
		PreviousKey:     previousKey,
		AccessTokenTTL:  15 * time.Minute,
		RefreshTokenTTL: 30 * 24 * time.Hour,
	})
	if err != nil {
		t.Fatalf("NewTokenService() error = %v", err)
	}
	return svc
}

func TestTokenService_IssueAndVerifyAccessToken(t *testing.T) {
	svc := newTestService(t, "test-signing-key-at-least-32-bytes!!", "")

	userID, sessionID := ids.New(), ids.New()
	pair, err := svc.IssuePair(userID, sessionID, "device-1", []string{"user"})
	if err != nil {
		t.Fatalf("IssuePair() error = %v", err)
	}

	claims, err := svc.VerifyAccessToken(context.Background(), pair.AccessToken)
	if err != nil {
		t.Fatalf("VerifyAccessToken() error = %v", err)
	}
	if claims.UserID != userID || claims.SessionID != sessionID || claims.DeviceID != "device-1" {
		t.Errorf("VerifyAccessToken() claims = %+v, want userID=%v sessionID=%v deviceID=device-1", claims, userID, sessionID)
	}
	if len(claims.Roles) != 1 || claims.Roles[0] != "user" {
		t.Errorf("VerifyAccessToken() roles = %v, want [user]", claims.Roles)
	}
}

func TestTokenService_RejectsRefreshTokenAsAccessToken(t *testing.T) {
	svc := newTestService(t, "test-signing-key-at-least-32-bytes!!", "")

	pair, err := svc.IssuePair(ids.New(), ids.New(), "device-1", nil)
	if err != nil {
		t.Fatalf("IssuePair() error = %v", err)
	}

	if _, err := svc.VerifyAccessToken(context.Background(), pair.RefreshToken); err == nil {
		t.Error("VerifyAccessToken() with a refresh token: want error, got nil")
	}
}

func TestTokenService_KeyRotation(t *testing.T) {
	oldKey := "old-signing-key-at-least-32-bytes!!"
	newKey := "new-signing-key-at-least-32-bytes!!"

	oldService := newTestService(t, oldKey, "")
	pair, err := oldService.IssuePair(ids.New(), ids.New(), "device-1", nil)
	if err != nil {
		t.Fatalf("IssuePair() error = %v", err)
	}

	// After rotation, the new signing key is primary but the old key is still
	// accepted for verification, so tokens issued before rotation stay valid
	// until they expire (see docs/decisions — zero-downtime key rotation).
	rotatedService := newTestService(t, newKey, oldKey)

	if _, err := rotatedService.VerifyAccessToken(context.Background(), pair.AccessToken); err != nil {
		t.Errorf("VerifyAccessToken() with pre-rotation token: got error %v, want nil", err)
	}

	newPair, err := rotatedService.IssuePair(ids.New(), ids.New(), "device-2", nil)
	if err != nil {
		t.Fatalf("IssuePair() on rotated service: %v", err)
	}
	if _, err := oldService.VerifyAccessToken(context.Background(), newPair.AccessToken); err == nil {
		t.Error("old service verified a token signed with the new key; want error")
	}
}
