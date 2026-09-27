package application_test

import (
	"context"
	"io"
	"log/slog"
	"testing"
	"time"

	authapi "github.com/aedst/server/internal/modules/auth/api"
	"github.com/aedst/server/internal/modules/users/application"
	"github.com/aedst/server/internal/modules/users/domain"
	sharederrors "github.com/aedst/server/internal/shared/errors"
	"github.com/aedst/server/internal/shared/ids"
	sharedtime "github.com/aedst/server/internal/shared/time"
)

// fakeRepository and fakeAuth are in-memory test doubles for application's
// ports, matching the convention in modules/auth/application/service_test.go
// (see docs/architecture.md — "Most business logic should be tested without
// infrastructure").

type fakeRepository struct {
	byID    map[ids.ID]domain.User
	byEmail map[string]domain.User
}

func newFakeRepository() *fakeRepository {
	return &fakeRepository{byID: map[ids.ID]domain.User{}, byEmail: map[string]domain.User{}}
}

func (f *fakeRepository) Create(_ context.Context, user domain.User) error {
	f.byID[user.ID] = user
	f.byEmail[user.Email] = user
	return nil
}

func (f *fakeRepository) FindByID(_ context.Context, id ids.ID) (domain.User, error) {
	user, ok := f.byID[id]
	if !ok {
		return domain.User{}, sharederrors.NotFound("fake.FindByID", "not found")
	}
	return user, nil
}

func (f *fakeRepository) FindByEmail(_ context.Context, email string) (domain.User, error) {
	user, ok := f.byEmail[email]
	if !ok {
		return domain.User{}, sharederrors.NotFound("fake.FindByEmail", "not found")
	}
	return user, nil
}

// fakeAuth is an in-memory double for authapi.Service, the only cross-module
// dependency users' application layer has (see docs/modules.md).
type fakeAuth struct {
	credentialsByEmail map[string]struct{}

	createCredentialErr error
	issueSessionErr     error
}

func newFakeAuth() *fakeAuth {
	return &fakeAuth{credentialsByEmail: map[string]struct{}{}}
}

func (f *fakeAuth) CreateCredential(_ context.Context, _ ids.ID, email, _ string) error {
	if f.createCredentialErr != nil {
		return f.createCredentialErr
	}
	if _, ok := f.credentialsByEmail[email]; ok {
		return sharederrors.AlreadyExists("fake.CreateCredential", "email already registered")
	}
	f.credentialsByEmail[email] = struct{}{}
	return nil
}

func (f *fakeAuth) IssueSession(_ context.Context, userID ids.ID, deviceID string) (authapi.TokenPair, error) {
	if f.issueSessionErr != nil {
		return authapi.TokenPair{}, f.issueSessionErr
	}
	return authapi.TokenPair{
		AccessToken:  "access-" + userID.String() + "-" + deviceID,
		RefreshToken: "refresh-" + userID.String() + "-" + deviceID,
		AccessTTL:    15 * time.Minute,
		RefreshTTL:   30 * 24 * time.Hour,
	}, nil
}

func (f *fakeAuth) Login(context.Context, string, string, string) (ids.ID, authapi.TokenPair, error) {
	return ids.ID{}, authapi.TokenPair{}, sharederrors.Unauthorized("fake.Login", "not implemented in fake")
}

func (f *fakeAuth) RefreshToken(context.Context, string) (authapi.TokenPair, error) {
	return authapi.TokenPair{}, sharederrors.Unauthorized("fake.RefreshToken", "not implemented in fake")
}

func (f *fakeAuth) Logout(context.Context, string) error {
	return sharederrors.Unauthorized("fake.Logout", "not implemented in fake")
}

// fakeEventPublisher records PublishUserRegistered calls (or fails them, to
// exercise the best-effort-publish path in RegisterUser).
type fakeEventPublisher struct {
	published []domain.User
	err       error
}

func (f *fakeEventPublisher) PublishUserRegistered(_ context.Context, user domain.User) error {
	if f.err != nil {
		return f.err
	}
	f.published = append(f.published, user)
	return nil
}

func testLogger() *slog.Logger {
	return slog.New(slog.NewTextHandler(io.Discard, nil))
}

func TestService_RegisterUser_Success(t *testing.T) {
	repo := newFakeRepository()
	auth := newFakeAuth()
	events := &fakeEventPublisher{}
	now := time.Now()
	svc := application.NewService(repo, auth, events, sharedtime.Fixed{At: now}, testLogger())

	user, pair, err := svc.RegisterUser(context.Background(), "a@example.com", "password123", "device-1")
	if err != nil {
		t.Fatalf("RegisterUser() error = %v", err)
	}
	if user.Email != "a@example.com" {
		t.Errorf("RegisterUser() email = %q, want %q", user.Email, "a@example.com")
	}
	if len(user.Roles) != 1 || user.Roles[0] != "user" {
		t.Errorf("RegisterUser() roles = %v, want [user]", user.Roles)
	}
	if !user.CreatedAt.Equal(now) {
		t.Errorf("RegisterUser() createdAt = %v, want %v", user.CreatedAt, now)
	}
	if pair.AccessToken == "" || pair.RefreshToken == "" {
		t.Errorf("RegisterUser() returned empty tokens: %+v", pair)
	}

	stored, err := repo.FindByID(context.Background(), user.ID)
	if err != nil {
		t.Fatalf("expected user to be persisted: %v", err)
	}
	if stored.Email != user.Email {
		t.Errorf("persisted user email = %q, want %q", stored.Email, user.Email)
	}

	if len(events.published) != 1 {
		t.Fatalf("expected exactly one UserRegistered publish, got %d", len(events.published))
	}
	if events.published[0].ID != user.ID {
		t.Errorf("published event userID = %v, want %v", events.published[0].ID, user.ID)
	}
}

func TestService_RegisterUser_DuplicateEmail(t *testing.T) {
	repo := newFakeRepository()
	auth := newFakeAuth()
	svc := application.NewService(repo, auth, &fakeEventPublisher{}, sharedtime.Fixed{At: time.Now()}, testLogger())

	ctx := context.Background()
	if _, _, err := svc.RegisterUser(ctx, "dup@example.com", "password123", "device-1"); err != nil {
		t.Fatalf("first RegisterUser: %v", err)
	}

	_, _, err := svc.RegisterUser(ctx, "dup@example.com", "password123", "device-2")
	if sharederrors.KindOf(err) != sharederrors.KindAlreadyExists {
		t.Fatalf("second RegisterUser: got err = %v, want KindAlreadyExists", err)
	}
}

func TestService_RegisterUser_CreateCredentialFails(t *testing.T) {
	repo := newFakeRepository()
	auth := newFakeAuth()
	auth.createCredentialErr = sharederrors.Validation("fake.CreateCredential", "password too weak")
	svc := application.NewService(repo, auth, &fakeEventPublisher{}, sharedtime.Fixed{At: time.Now()}, testLogger())

	_, _, err := svc.RegisterUser(context.Background(), "a@example.com", "weak", "device-1")
	if sharederrors.KindOf(err) != sharederrors.KindValidation {
		t.Fatalf("RegisterUser() with failing CreateCredential: got err = %v, want KindValidation", err)
	}

	if _, err := repo.FindByEmail(context.Background(), "a@example.com"); sharederrors.KindOf(err) != sharederrors.KindNotFound {
		t.Errorf("expected no user to be persisted when CreateCredential fails, got err = %v", err)
	}
}

func TestService_RegisterUser_IssueSessionFails(t *testing.T) {
	repo := newFakeRepository()
	auth := newFakeAuth()
	auth.issueSessionErr = sharederrors.Wrap("fake.IssueSession", context.DeadlineExceeded)
	svc := application.NewService(repo, auth, &fakeEventPublisher{}, sharedtime.Fixed{At: time.Now()}, testLogger())

	_, _, err := svc.RegisterUser(context.Background(), "a@example.com", "password123", "device-1")
	if err == nil {
		t.Fatal("RegisterUser() with failing IssueSession: expected error, got nil")
	}

	// The identity record is still created even though session issuance
	// failed (see Service.RegisterUser's comment on partial-failure
	// semantics): the user exists but has no session yet.
	if _, err := repo.FindByEmail(context.Background(), "a@example.com"); err != nil {
		t.Errorf("expected user to still be persisted when IssueSession fails: %v", err)
	}
}

func TestService_RegisterUser_PublishFailureDoesNotFailRegistration(t *testing.T) {
	repo := newFakeRepository()
	auth := newFakeAuth()
	events := &fakeEventPublisher{err: context.DeadlineExceeded}
	svc := application.NewService(repo, auth, events, sharedtime.Fixed{At: time.Now()}, testLogger())

	user, pair, err := svc.RegisterUser(context.Background(), "a@example.com", "password123", "device-1")
	if err != nil {
		t.Fatalf("RegisterUser() should succeed even if publishing fails: %v", err)
	}
	if user.Email == "" || pair.AccessToken == "" {
		t.Errorf("RegisterUser() returned incomplete result: user=%+v pair=%+v", user, pair)
	}
}

func TestService_GetUser(t *testing.T) {
	repo := newFakeRepository()
	svc := application.NewService(repo, newFakeAuth(), &fakeEventPublisher{}, sharedtime.Fixed{At: time.Now()}, testLogger())

	userID := ids.New()
	seeded := domain.User{ID: userID, Email: "a@example.com", Roles: []string{"user"}, CreatedAt: time.Now()}
	if err := repo.Create(context.Background(), seeded); err != nil {
		t.Fatalf("seed user: %v", err)
	}

	got, err := svc.GetUser(context.Background(), userID)
	if err != nil {
		t.Fatalf("GetUser() error = %v", err)
	}
	if got.Email != seeded.Email {
		t.Errorf("GetUser() email = %q, want %q", got.Email, seeded.Email)
	}
}

func TestService_GetUser_NotFound(t *testing.T) {
	svc := application.NewService(newFakeRepository(), newFakeAuth(), &fakeEventPublisher{}, sharedtime.Fixed{At: time.Now()}, testLogger())

	_, err := svc.GetUser(context.Background(), ids.New())
	if sharederrors.KindOf(err) != sharederrors.KindNotFound {
		t.Fatalf("GetUser() for unknown id: got err = %v, want KindNotFound", err)
	}
}

func TestService_GetUserByEmail(t *testing.T) {
	repo := newFakeRepository()
	svc := application.NewService(repo, newFakeAuth(), &fakeEventPublisher{}, sharedtime.Fixed{At: time.Now()}, testLogger())

	userID := ids.New()
	seeded := domain.User{ID: userID, Email: "a@example.com", Roles: []string{"user"}, CreatedAt: time.Now()}
	if err := repo.Create(context.Background(), seeded); err != nil {
		t.Fatalf("seed user: %v", err)
	}

	got, err := svc.GetUserByEmail(context.Background(), "a@example.com")
	if err != nil {
		t.Fatalf("GetUserByEmail() error = %v", err)
	}
	if got.ID != userID {
		t.Errorf("GetUserByEmail() id = %v, want %v", got.ID, userID)
	}
}

func TestService_GetUserByEmail_NotFound(t *testing.T) {
	svc := application.NewService(newFakeRepository(), newFakeAuth(), &fakeEventPublisher{}, sharedtime.Fixed{At: time.Now()}, testLogger())

	_, err := svc.GetUserByEmail(context.Background(), "nobody@example.com")
	if sharederrors.KindOf(err) != sharederrors.KindNotFound {
		t.Fatalf("GetUserByEmail() for unknown email: got err = %v, want KindNotFound", err)
	}
}
