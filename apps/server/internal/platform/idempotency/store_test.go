package idempotency_test

import (
	"context"
	"testing"

	"github.com/jackc/pgx/v5"

	"github.com/aedst/server/internal/platform/idempotency"
	sharederrors "github.com/aedst/server/internal/shared/errors"
)

type storeEntry struct {
	result []byte
	hash   string
}

type fakeStore struct {
	byKey map[string]storeEntry
}

func newFakeStore() *fakeStore {
	return &fakeStore{byKey: map[string]storeEntry{}}
}

func (f *fakeStore) Get(_ context.Context, _ pgx.Tx, key string) ([]byte, string, bool, error) {
	entry, ok := f.byKey[key]
	if !ok {
		return nil, "", false, nil
	}
	return entry.result, entry.hash, true, nil
}

func (f *fakeStore) Save(_ context.Context, _ pgx.Tx, key, requestHash string, result []byte) error {
	f.byKey[key] = storeEntry{result: result, hash: requestHash}
	return nil
}

func TestCheck_NoKey_ProceedsNormally(t *testing.T) {
	store := newFakeStore()
	result, replay, err := idempotency.Check(context.Background(), nil, store, "", []byte("payload"))
	if err != nil {
		t.Fatalf("Check: %v", err)
	}
	if replay || result != nil {
		t.Fatalf("expected no replay for empty key, got replay=%v result=%v", replay, result)
	}
}

func TestCheck_FirstSeen_ThenSave_ThenReplay(t *testing.T) {
	store := newFakeStore()
	ctx := context.Background()
	payload := []byte(`{"title":"buy milk"}`)

	result, replay, err := idempotency.Check(ctx, nil, store, "key-1", payload)
	if err != nil {
		t.Fatalf("Check: %v", err)
	}
	if replay || result != nil {
		t.Fatalf("expected first call to be a miss, got replay=%v result=%v", replay, result)
	}

	computed := []byte(`{"id":"task-1"}`)
	if err := idempotency.Save(ctx, nil, store, "key-1", payload, computed); err != nil {
		t.Fatalf("Save: %v", err)
	}

	result, replay, err = idempotency.Check(ctx, nil, store, "key-1", payload)
	if err != nil {
		t.Fatalf("Check (replay): %v", err)
	}
	if !replay {
		t.Fatalf("expected replay on second call with the same key and payload")
	}
	if string(result) != string(computed) {
		t.Fatalf("expected cached result %s, got %s", computed, result)
	}
}

func TestCheck_SameKeyDifferentPayload_Conflict(t *testing.T) {
	store := newFakeStore()
	ctx := context.Background()

	if _, _, err := idempotency.Check(ctx, nil, store, "key-1", []byte("a")); err != nil {
		t.Fatalf("Check: %v", err)
	}
	if err := idempotency.Save(ctx, nil, store, "key-1", []byte("a"), []byte("result-a")); err != nil {
		t.Fatalf("Save: %v", err)
	}

	_, _, err := idempotency.Check(ctx, nil, store, "key-1", []byte("b"))
	if err == nil {
		t.Fatal("expected a conflict error for a reused key with a different payload")
	}
	if sharederrors.KindOf(err) != sharederrors.KindConflict {
		t.Fatalf("expected KindConflict, got %v", sharederrors.KindOf(err))
	}
}

func TestSave_NoKey_NoOp(t *testing.T) {
	store := newFakeStore()
	if err := idempotency.Save(context.Background(), nil, store, "", []byte("a"), []byte("b")); err != nil {
		t.Fatalf("Save: %v", err)
	}
	if len(store.byKey) != 0 {
		t.Fatalf("expected no entry saved for an empty key")
	}
}
