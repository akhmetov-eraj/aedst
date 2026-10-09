package application_test

import (
	"context"
	"strconv"
	"strings"
	"testing"
	"time"

	"github.com/aedst/server/internal/modules/auth/application"
	"github.com/aedst/server/internal/modules/auth/domain"
	sharederrors "github.com/aedst/server/internal/shared/errors"
	"github.com/aedst/server/internal/shared/ids"
	sharedtime "github.com/aedst/server/internal/shared/time"
)

// fakeCredentials/fakeSessions/fakeTokens/fakeHasher are in-memory test
// doubles for application's ports, demonstrating that use cases are testable
// without a database or a real JWT library (see docs/content/server/architecture.md — "Most
// business logic should be tested without infrastructure").

type fakeCredentials struct {
	byEmail map[string]domain.Credential
}

func newFakeCredentials() *fakeCredentials {
	return &fakeCredentials{byEmail: map[string]domain.Credential{}}
}

func (f *fakeCredentials) Create(_ context.Context, cred domain.Credential) error {
	if _, ok := f.byEmail[cred.Email]; ok {
		return sharederrors.AlreadyExists("fake.CreateCredential", "email already registered")
	}
	f.byEmail[cred.Email] = cred
	return nil
}

func (f *fakeCredentials) FindByEmail(_ context.Context, email string) (domain.Credential, error) {
	cred, ok := f.byEmail[email]
	if !ok {
		return domain.Credential{}, sharederrors.NotFound("fake.FindByEmail", "not found")
	}
	return cred, nil
}

func (f *fakeCredentials) UpdatePasswordHash(_ context.Context, userID ids.ID, hash string) error {
	for email, cred := range f.byEmail {
		if cred.UserID == userID {
			cred.PasswordHash = hash
			f.byEmail[email] = cred
		}
	}
	return nil
}

func (f *fakeCredentials) FindByUserID(_ context.Context, userID ids.ID) (domain.Credential, error) {
	for _, cred := range f.byEmail {
		if cred.UserID == userID {
			return cred, nil
		}
	}
	return domain.Credential{}, sharederrors.NotFound("fake.FindByUserID", "not found")
}

type fakeSessions struct {
	byID map[ids.ID]domain.Session
}

func newFakeSessions() *fakeSessions {
	return &fakeSessions{byID: map[ids.ID]domain.Session{}}
}

func (f *fakeSessions) Create(_ context.Context, session domain.Session) error {
	f.byID[session.ID] = session
	return nil
}

func (f *fakeSessions) FindByID(_ context.Context, id ids.ID) (domain.Session, error) {
	session, ok := f.byID[id]
	if !ok {
		return domain.Session{}, sharederrors.NotFound("fake.FindByID", "not found")
	}
	return session, nil
}

func (f *fakeSessions) Rotate(_ context.Context, id ids.ID, oldHash, newHash string, newExpiresAt time.Time) (bool, error) {
	session := f.byID[id]
	if session.RevokedAt != nil || session.RefreshTokenHash != oldHash {
		return false, nil
	}
	session.RefreshTokenHash = newHash
	session.ExpiresAt = newExpiresAt
	f.byID[id] = session
	return true, nil
}

func (f *fakeSessions) Revoke(_ context.Context, id ids.ID, revokedAt time.Time) error {
	session := f.byID[id]
	session.RevokedAt = &revokedAt
	f.byID[id] = session
	return nil
}

// fakeTokens issues predictable, incrementing "tokens" instead of real JWTs.
type fakeTokens struct{ counter int }

func (f *fakeTokens) IssuePair(userID, sessionID ids.ID, deviceID string, roles []string) (application.TokenPair, error) {
	f.counter++
	return application.TokenPair{
		AccessToken:  "access-" + sessionID.String(),
		RefreshToken: "refresh-" + sessionID.String() + "-" + strconv.Itoa(f.counter),
		AccessTTL:    15 * time.Minute,
		RefreshTTL:   30 * 24 * time.Hour,
	}, nil
}

func (f *fakeTokens) VerifyRefreshToken(_ context.Context, raw string) (application.RefreshClaims, error) {
	// Fake refresh tokens look like "refresh-<sessionID>-<counter>".
	rest, ok := strings.CutPrefix(raw, "refresh-")
	if !ok || len(rest) < 36 {
		return application.RefreshClaims{}, sharederrors.Unauthorized("fake.VerifyRefreshToken", "malformed")
	}
	sessionID, err := ids.Parse(rest[:36])
	if err != nil {
		return application.RefreshClaims{}, sharederrors.Unauthorized("fake.VerifyRefreshToken", "malformed")
	}
	return application.RefreshClaims{SessionID: sessionID}, nil
}

type fakeHasher struct{ dummyCalls *int }

func (fakeHasher) Hash(password string) (string, error) { return "hashed:" + password, nil }
func (fakeHasher) Verify(hash, password string) bool {
	return hash == "hashed:"+password || hash == "legacy:"+password
}

func (fakeHasher) NeedsRehash(hash string) bool { return strings.HasPrefix(hash, "legacy:") }

func (h fakeHasher) VerifyDummy(string) {
	if h.dummyCalls != nil {
		*h.dummyCalls++
	}
}

func TestService_Login_Success(t *testing.T) {
	creds := newFakeCredentials()
	sessions := newFakeSessions()
	svc := application.NewService(creds, sessions, &fakeTokens{}, fakeHasher{}, sharedtime.Fixed{At: time.Now()})

	userID := ids.New()
	if err := creds.Create(context.Background(), domain.Credential{
		UserID:       userID,
		Email:        "a@example.com",
		PasswordHash: "hashed:correct-password",
	}); err != nil {
		t.Fatalf("seed credential: %v", err)
	}

	gotUserID, pair, err := svc.Login(context.Background(), "a@example.com", "correct-password", "device-1")
	if err != nil {
		t.Fatalf("Login() error = %v", err)
	}
	if gotUserID != userID {
		t.Errorf("Login() userID = %v, want %v", gotUserID, userID)
	}
	if pair.AccessToken == "" || pair.RefreshToken == "" {
		t.Errorf("Login() returned empty tokens: %+v", pair)
	}
	if len(sessions.byID) != 1 {
		t.Errorf("expected exactly one session to be created, got %d", len(sessions.byID))
	}
}

func TestService_Login_WrongPassword(t *testing.T) {
	creds := newFakeCredentials()
	sessions := newFakeSessions()
	svc := application.NewService(creds, sessions, &fakeTokens{}, fakeHasher{}, sharedtime.Fixed{At: time.Now()})

	_ = creds.Create(context.Background(), domain.Credential{
		UserID:       ids.New(),
		Email:        "a@example.com",
		PasswordHash: "hashed:correct-password",
	})

	_, _, err := svc.Login(context.Background(), "a@example.com", "wrong-password", "device-1")
	if sharederrors.KindOf(err) != sharederrors.KindUnauthorized {
		t.Fatalf("Login() with wrong password: got err = %v, want KindUnauthorized", err)
	}
}

func TestService_Login_UnknownEmail(t *testing.T) {
	svc := application.NewService(newFakeCredentials(), newFakeSessions(), &fakeTokens{}, fakeHasher{}, sharedtime.Fixed{At: time.Now()})

	_, _, err := svc.Login(context.Background(), "nobody@example.com", "whatever", "device-1")
	if sharederrors.KindOf(err) != sharederrors.KindUnauthorized {
		t.Fatalf("Login() with unknown email: got err = %v, want KindUnauthorized", err)
	}
}

func TestService_Register_Success(t *testing.T) {
	creds := newFakeCredentials()
	sessions := newFakeSessions()
	svc := application.NewService(creds, sessions, &fakeTokens{}, fakeHasher{}, sharedtime.Fixed{At: time.Now()})

	user, pair, err := svc.Register(context.Background(), "new@example.com", "password123", "device-1")
	if err != nil {
		t.Fatalf("Register() error = %v", err)
	}
	if user.Email != "new@example.com" {
		t.Errorf("Register() user.Email = %q, want %q", user.Email, "new@example.com")
	}
	if user.ID.IsNil() {
		t.Error("Register() returned a nil user ID")
	}
	if pair.AccessToken == "" || pair.RefreshToken == "" {
		t.Errorf("Register() returned empty tokens: %+v", pair)
	}
	if len(sessions.byID) != 1 {
		t.Errorf("expected exactly one session to be created, got %d", len(sessions.byID))
	}

	stored, err := creds.FindByEmail(context.Background(), "new@example.com")
	if err != nil {
		t.Fatalf("credential was not persisted: %v", err)
	}
	if stored.PasswordHash == "password123" {
		t.Error("password was stored in plaintext")
	}
}

func TestService_Register_DuplicateEmail(t *testing.T) {
	creds := newFakeCredentials()
	svc := application.NewService(creds, newFakeSessions(), &fakeTokens{}, fakeHasher{}, sharedtime.Fixed{At: time.Now()})

	ctx := context.Background()
	if _, _, err := svc.Register(ctx, "dup@example.com", "password123", "device-1"); err != nil {
		t.Fatalf("first Register: %v", err)
	}

	_, _, err := svc.Register(ctx, "dup@example.com", "password123", "device-2")
	if sharederrors.KindOf(err) != sharederrors.KindAlreadyExists {
		t.Fatalf("second Register: got err = %v, want KindAlreadyExists", err)
	}
}

func TestService_GetCurrentUser(t *testing.T) {
	creds := newFakeCredentials()
	svc := application.NewService(creds, newFakeSessions(), &fakeTokens{}, fakeHasher{}, sharedtime.Fixed{At: time.Now()})

	ctx := context.Background()
	registered, _, err := svc.Register(ctx, "me@example.com", "password123", "device-1")
	if err != nil {
		t.Fatalf("Register: %v", err)
	}

	got, err := svc.GetCurrentUser(ctx, registered.ID)
	if err != nil {
		t.Fatalf("GetCurrentUser() error = %v", err)
	}
	if got.Email != "me@example.com" {
		t.Errorf("GetCurrentUser() email = %q, want %q", got.Email, "me@example.com")
	}

	if _, err := svc.GetCurrentUser(ctx, ids.New()); sharederrors.KindOf(err) != sharederrors.KindNotFound {
		t.Fatalf("GetCurrentUser() for unknown id: got err = %v, want KindNotFound", err)
	}
}

func TestService_Login_UnknownEmailStillBurnsAHash(t *testing.T) {
	calls := 0
	svc := application.NewService(newFakeCredentials(), newFakeSessions(), &fakeTokens{}, fakeHasher{dummyCalls: &calls}, sharedtime.Fixed{At: time.Now()})

	_, _, _ = svc.Login(context.Background(), "nobody@example.com", "whatever", "d")
	if calls != 1 {
		t.Fatalf("VerifyDummy calls = %d, want 1 (unknown accounts must cost the same as real ones)", calls)
	}
}

func TestService_Login_UpgradesLegacyHash(t *testing.T) {
	creds := newFakeCredentials()
	svc := application.NewService(creds, newFakeSessions(), &fakeTokens{}, fakeHasher{}, sharedtime.Fixed{At: time.Now()})
	_ = creds.Create(context.Background(), domain.Credential{UserID: ids.New(), Email: "a@example.com", PasswordHash: "legacy:pw12345678"})

	if _, _, err := svc.Login(context.Background(), "a@example.com", "pw12345678", "d"); err != nil {
		t.Fatalf("Login() error = %v", err)
	}
	if got := creds.byEmail["a@example.com"].PasswordHash; got != "hashed:pw12345678" {
		t.Fatalf("stored hash = %q, want the upgraded hash", got)
	}
}

func TestService_EmailIsCaseInsensitive(t *testing.T) {
	svc := application.NewService(newFakeCredentials(), newFakeSessions(), &fakeTokens{}, fakeHasher{}, sharedtime.Fixed{At: time.Now()})
	ctx := context.Background()

	if _, _, err := svc.Register(ctx, "Mixed@Example.com", "password123", "d"); err != nil {
		t.Fatalf("Register: %v", err)
	}
	if _, _, err := svc.Login(ctx, " mixed@example.COM ", "password123", "d"); err != nil {
		t.Fatalf("Login with different case: %v", err)
	}
	if _, _, err := svc.Register(ctx, "MIXED@example.com", "password123", "d"); sharederrors.KindOf(err) != sharederrors.KindAlreadyExists {
		t.Fatalf("duplicate Register with different case: err = %v, want KindAlreadyExists", err)
	}
}

type denyLimiter struct{}

func (denyLimiter) Allow(context.Context, string) (bool, error) { return false, nil }

func TestService_Login_RateLimited(t *testing.T) {
	svc := application.NewService(newFakeCredentials(), newFakeSessions(), &fakeTokens{}, fakeHasher{}, sharedtime.Fixed{At: time.Now()}, application.WithLoginLimiter(denyLimiter{}))

	_, _, err := svc.Login(context.Background(), "a@example.com", "x", "d")
	if sharederrors.KindOf(err) != sharederrors.KindRateLimited {
		t.Fatalf("Login() err = %v, want KindRateLimited", err)
	}
}

func TestService_RefreshToken_RotatesAndDetectsReuse(t *testing.T) {
	sessions := newFakeSessions()
	svc := application.NewService(newFakeCredentials(), sessions, &fakeTokens{}, fakeHasher{}, sharedtime.Fixed{At: time.Now()})
	ctx := context.Background()

	_, first, err := svc.Register(ctx, "r@example.com", "password123", "d")
	if err != nil {
		t.Fatalf("Register: %v", err)
	}

	second, err := svc.RefreshToken(ctx, first.RefreshToken)
	if err != nil {
		t.Fatalf("RefreshToken() error = %v", err)
	}
	if second.RefreshToken == first.RefreshToken {
		t.Fatal("refresh token was not rotated")
	}

	// Replaying the already-rotated token is theft/reuse: it must fail AND
	// kill the session, so the thief's newer token dies too.
	if _, err := svc.RefreshToken(ctx, first.RefreshToken); sharederrors.KindOf(err) != sharederrors.KindUnauthorized {
		t.Fatalf("replayed RefreshToken() err = %v, want KindUnauthorized", err)
	}
	if _, err := svc.RefreshToken(ctx, second.RefreshToken); sharederrors.KindOf(err) != sharederrors.KindUnauthorized {
		t.Fatalf("RefreshToken() with the legitimate newer token after reuse err = %v, want it revoked", err)
	}
}

func TestService_Logout_RevokesTheSessionAndIsIdempotent(t *testing.T) {
	svc := application.NewService(newFakeCredentials(), newFakeSessions(), &fakeTokens{}, fakeHasher{}, sharedtime.Fixed{At: time.Now()})
	ctx := context.Background()

	_, pair, err := svc.Register(ctx, "l@example.com", "password123", "d")
	if err != nil {
		t.Fatalf("Register: %v", err)
	}
	if err := svc.Logout(ctx, pair.RefreshToken); err != nil {
		t.Fatalf("Logout: %v", err)
	}
	if _, err := svc.RefreshToken(ctx, pair.RefreshToken); sharederrors.KindOf(err) != sharederrors.KindUnauthorized {
		t.Fatalf("RefreshToken() after Logout = %v, want Unauthorized (the session is revoked)", err)
	}
	// Logging out again, or with garbage, is not an error: there is nothing left to revoke.
	if err := svc.Logout(ctx, pair.RefreshToken); err != nil {
		t.Errorf("second Logout = %v, want nil", err)
	}
	if err := svc.Logout(ctx, "garbage"); err != nil {
		t.Errorf("Logout(garbage) = %v, want nil", err)
	}
}
