package application_test

import (
	"context"
	"testing"
	"time"

	"github.com/aedst/server/internal/modules/profile/application"
	"github.com/aedst/server/internal/modules/profile/domain"
	sharederrors "github.com/aedst/server/internal/shared/errors"
	"github.com/aedst/server/internal/shared/ids"
	sharedtime "github.com/aedst/server/internal/shared/time"
)

// fakeRepository and fakeEventPublisher are in-memory test doubles for
// application's ports, matching the convention in
// modules/auth/application/service_test.go (see docs/architecture.md —
// "Most business logic should be tested without infrastructure").

type fakeRepository struct {
	byUserID map[ids.ID]domain.Profile
}

func newFakeRepository() *fakeRepository {
	return &fakeRepository{byUserID: map[ids.ID]domain.Profile{}}
}

func (f *fakeRepository) Upsert(_ context.Context, profile domain.Profile) (domain.Profile, error) {
	f.byUserID[profile.UserID] = profile
	return profile, nil
}

func (f *fakeRepository) FindByUserID(_ context.Context, userID ids.ID) (domain.Profile, error) {
	profile, ok := f.byUserID[userID]
	if !ok {
		return domain.Profile{}, sharederrors.NotFound("fake.FindByUserID", "not found")
	}
	return profile, nil
}

type fakeEventPublisher struct {
	published []domain.Profile
	err       error
}

func (f *fakeEventPublisher) PublishProfileUpdated(_ context.Context, profile domain.Profile) error {
	if f.err != nil {
		return f.err
	}
	f.published = append(f.published, profile)
	return nil
}

func TestService_CreateDefault(t *testing.T) {
	repo := newFakeRepository()
	now := time.Now()
	svc := application.NewService(repo, &fakeEventPublisher{}, sharedtime.Fixed{At: now})

	userID := ids.New()
	profile, err := svc.CreateDefault(context.Background(), userID, "Jane")
	if err != nil {
		t.Fatalf("CreateDefault() error = %v", err)
	}
	if profile.UserID != userID {
		t.Errorf("CreateDefault() userID = %v, want %v", profile.UserID, userID)
	}
	if profile.DisplayName != "Jane" {
		t.Errorf("CreateDefault() displayName = %q, want %q", profile.DisplayName, "Jane")
	}
	if !profile.UpdatedAt.Equal(now) {
		t.Errorf("CreateDefault() updatedAt = %v, want %v", profile.UpdatedAt, now)
	}

	stored, err := repo.FindByUserID(context.Background(), userID)
	if err != nil {
		t.Fatalf("expected profile to be persisted: %v", err)
	}
	if stored.DisplayName != "Jane" {
		t.Errorf("persisted displayName = %q, want %q", stored.DisplayName, "Jane")
	}
}

func TestService_UpdateProfile_Success(t *testing.T) {
	repo := newFakeRepository()
	events := &fakeEventPublisher{}
	now := time.Now()
	svc := application.NewService(repo, events, sharedtime.Fixed{At: now})

	userID := ids.New()
	profile, err := svc.UpdateProfile(context.Background(), userID, "Jane", "hello world", "https://example.com/a.png")
	if err != nil {
		t.Fatalf("UpdateProfile() error = %v", err)
	}
	if profile.Bio != "hello world" || profile.AvatarURL != "https://example.com/a.png" {
		t.Errorf("UpdateProfile() = %+v, want bio/avatar set", profile)
	}
	if !profile.UpdatedAt.Equal(now) {
		t.Errorf("UpdateProfile() updatedAt = %v, want %v", profile.UpdatedAt, now)
	}

	if len(events.published) != 1 {
		t.Fatalf("expected exactly one ProfileUpdated publish, got %d", len(events.published))
	}
	if events.published[0].UserID != userID {
		t.Errorf("published event userID = %v, want %v", events.published[0].UserID, userID)
	}
}

func TestService_UpdateProfile_PublishFailurePropagates(t *testing.T) {
	repo := newFakeRepository()
	events := &fakeEventPublisher{err: context.DeadlineExceeded}
	svc := application.NewService(repo, events, sharedtime.Fixed{At: time.Now()})

	_, err := svc.UpdateProfile(context.Background(), ids.New(), "Jane", "bio", "")
	if err == nil {
		t.Fatal("UpdateProfile() expected error when publish fails, got nil")
	}
}

func TestService_UpdateProfile_RepositoryFailurePropagates(t *testing.T) {
	repo := &failingRepository{err: sharederrors.Wrap("fake.Upsert", context.DeadlineExceeded)}
	events := &fakeEventPublisher{}
	svc := application.NewService(repo, events, sharedtime.Fixed{At: time.Now()})

	_, err := svc.UpdateProfile(context.Background(), ids.New(), "Jane", "bio", "")
	if err == nil {
		t.Fatal("UpdateProfile() expected error when repository fails, got nil")
	}
	if len(events.published) != 0 {
		t.Errorf("expected no publish when repository Upsert fails, got %d", len(events.published))
	}
}

// failingRepository always fails Upsert, used to verify UpdateProfile doesn't
// publish an event when persistence fails.
type failingRepository struct {
	err error
}

func (f *failingRepository) Upsert(context.Context, domain.Profile) (domain.Profile, error) {
	return domain.Profile{}, f.err
}

func (f *failingRepository) FindByUserID(context.Context, ids.ID) (domain.Profile, error) {
	return domain.Profile{}, f.err
}

func TestService_GetProfile(t *testing.T) {
	repo := newFakeRepository()
	svc := application.NewService(repo, &fakeEventPublisher{}, sharedtime.Fixed{At: time.Now()})

	userID := ids.New()
	seeded := domain.Profile{UserID: userID, DisplayName: "Jane", UpdatedAt: time.Now()}
	if _, err := repo.Upsert(context.Background(), seeded); err != nil {
		t.Fatalf("seed profile: %v", err)
	}

	got, err := svc.GetProfile(context.Background(), userID)
	if err != nil {
		t.Fatalf("GetProfile() error = %v", err)
	}
	if got.DisplayName != "Jane" {
		t.Errorf("GetProfile() displayName = %q, want %q", got.DisplayName, "Jane")
	}
}

func TestService_GetProfile_NotFound(t *testing.T) {
	svc := application.NewService(newFakeRepository(), &fakeEventPublisher{}, sharedtime.Fixed{At: time.Now()})

	_, err := svc.GetProfile(context.Background(), ids.New())
	if sharederrors.KindOf(err) != sharederrors.KindNotFound {
		t.Fatalf("GetProfile() for unknown user: got err = %v, want KindNotFound", err)
	}
}
