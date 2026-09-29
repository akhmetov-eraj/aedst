package application_test

import (
	"context"
	"strconv"
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
// without a database or a real JWT library (see docs/architecture.md — "Most
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

func (f *fakeSessions) Rotate(_ context.Context, id ids.ID, newHash string, newExpiresAt time.Time) error {
	session := f.byID[id]
	session.RefreshTokenHash = newHash
	session.ExpiresAt = newExpiresAt
	f.byID[id] = session
	return nil
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
	// The fake encodes nothing; tests that need real session linkage track the
	// session ID separately and call IssueSession/RefreshToken by ID directly
	// via the exported behavior instead of decoding this fake token string.
	return application.RefreshClaims{}, sharederrors.Unauthorized("fake.VerifyRefreshToken", "not implemented in fake")
}

type fakeHasher struct{}

func (fakeHasher) Hash(password string) (string, error) { return "hashed:" + password, nil }
func (fakeHasher) Verify(hash, password string) bool    { return hash == "hashed:"+password }

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
