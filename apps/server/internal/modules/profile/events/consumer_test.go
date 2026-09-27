package events_test

import (
	"context"
	"errors"
	"testing"

	"github.com/riverqueue/river"
	"github.com/riverqueue/river/rivertype"
	"google.golang.org/protobuf/proto"

	usersv1 "github.com/aedst/server/gen/go/aedst/users/v1"
	"github.com/aedst/server/internal/modules/profile/domain"
	"github.com/aedst/server/internal/modules/profile/events"
	profilejobs "github.com/aedst/server/internal/modules/profile/jobs"
	"github.com/aedst/server/internal/shared/ids"
)

// fakeCreator and fakeJobInserter are in-memory test doubles for
// HandleUserRegistered's dependencies, matching the convention in
// modules/auth/application/service_test.go (see docs/architecture.md —
// "Most business logic should be tested without infrastructure"). They let
// the consumer's decode-and-dispatch logic be tested without a running
// NATS/JetStream server.

type fakeCreator struct {
	created []struct {
		userID      ids.ID
		displayName string
	}
	err error
}

func (f *fakeCreator) CreateDefault(_ context.Context, userID ids.ID, displayName string) (domain.Profile, error) {
	if f.err != nil {
		return domain.Profile{}, f.err
	}
	f.created = append(f.created, struct {
		userID      ids.ID
		displayName string
	}{userID, displayName})
	return domain.Profile{UserID: userID, DisplayName: displayName}, nil
}

type fakeJobInserter struct {
	inserted []river.JobArgs
	err      error
}

func (f *fakeJobInserter) Insert(_ context.Context, args river.JobArgs, _ *river.InsertOpts) (*rivertype.JobInsertResult, error) {
	if f.err != nil {
		return nil, f.err
	}
	f.inserted = append(f.inserted, args)
	return &rivertype.JobInsertResult{}, nil
}

func marshalUserRegistered(t *testing.T, userID, email string) []byte {
	t.Helper()
	data, err := proto.Marshal(&usersv1.UserRegistered{
		EventId: ids.New().String(),
		UserId:  userID,
		Email:   email,
	})
	if err != nil {
		t.Fatalf("marshal UserRegistered: %v", err)
	}
	return data
}

func TestHandleUserRegistered_Success(t *testing.T) {
	creator := &fakeCreator{}
	jobs := &fakeJobInserter{}
	userID := ids.New()

	err := events.HandleUserRegistered(context.Background(), marshalUserRegistered(t, userID.String(), "a@example.com"), creator, jobs)
	if err != nil {
		t.Fatalf("HandleUserRegistered() error = %v", err)
	}

	if len(creator.created) != 1 {
		t.Fatalf("expected exactly one CreateDefault call, got %d", len(creator.created))
	}
	if creator.created[0].userID != userID {
		t.Errorf("CreateDefault userID = %v, want %v", creator.created[0].userID, userID)
	}

	if len(jobs.inserted) != 1 {
		t.Fatalf("expected exactly one welcome-email job to be enqueued, got %d", len(jobs.inserted))
	}
	welcome, ok := jobs.inserted[0].(profilejobs.WelcomeEmailArgs)
	if !ok {
		t.Fatalf("enqueued job args type = %T, want profilejobs.WelcomeEmailArgs", jobs.inserted[0])
	}
	if welcome.UserID != userID.String() || welcome.Email != "a@example.com" {
		t.Errorf("welcome job args = %+v, want userID=%s email=a@example.com", welcome, userID)
	}
}

func TestHandleUserRegistered_InvalidUserID(t *testing.T) {
	creator := &fakeCreator{}
	jobs := &fakeJobInserter{}

	err := events.HandleUserRegistered(context.Background(), marshalUserRegistered(t, "not-a-uuid", "a@example.com"), creator, jobs)
	if err == nil {
		t.Fatal("HandleUserRegistered() with invalid user_id: expected error, got nil")
	}
	if len(creator.created) != 0 {
		t.Errorf("expected no CreateDefault call for an invalid user_id, got %d", len(creator.created))
	}
	if len(jobs.inserted) != 0 {
		t.Errorf("expected no job to be enqueued for an invalid user_id, got %d", len(jobs.inserted))
	}
}

func TestHandleUserRegistered_CreateDefaultFails(t *testing.T) {
	creator := &fakeCreator{err: errors.New("boom")}
	jobs := &fakeJobInserter{}

	err := events.HandleUserRegistered(context.Background(), marshalUserRegistered(t, ids.New().String(), "a@example.com"), creator, jobs)
	if err == nil {
		t.Fatal("HandleUserRegistered() expected error when CreateDefault fails, got nil")
	}
	if len(jobs.inserted) != 0 {
		t.Errorf("expected no job to be enqueued when CreateDefault fails, got %d", len(jobs.inserted))
	}
}

func TestHandleUserRegistered_JobInsertFails(t *testing.T) {
	creator := &fakeCreator{}
	jobs := &fakeJobInserter{err: errors.New("insert failed")}

	err := events.HandleUserRegistered(context.Background(), marshalUserRegistered(t, ids.New().String(), "a@example.com"), creator, jobs)
	if err == nil {
		t.Fatal("HandleUserRegistered() expected error when job insert fails, got nil")
	}
	// The profile was still created even though enqueueing the follow-up job
	// failed; HandleUserRegistered doesn't roll that back (see the retry
	// semantics of the JetStream consumer this feeds).
	if len(creator.created) != 1 {
		t.Errorf("expected CreateDefault to have run before the job insert failure, got %d calls", len(creator.created))
	}
}

func TestHandleUserRegistered_NilJobInserter(t *testing.T) {
	creator := &fakeCreator{}

	err := events.HandleUserRegistered(context.Background(), marshalUserRegistered(t, ids.New().String(), "a@example.com"), creator, nil)
	if err != nil {
		t.Fatalf("HandleUserRegistered() with nil JobInserter: error = %v, want nil", err)
	}
	if len(creator.created) != 1 {
		t.Errorf("expected CreateDefault to still run with a nil JobInserter, got %d calls", len(creator.created))
	}
}
