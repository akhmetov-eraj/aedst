// Package idempotency lets a command handler safely replay a client retry
// of the same request instead of executing it twice. A client sends an
// Idempotency-Key header; the handler looks it up inside the same
// transaction as its own write, and either proceeds (first time) or returns
// the previously computed result (retry) — the check, the write, and the
// save all commit atomically, so there's no window where a crash between
// "did the work" and "recorded the key" could let a retry redo it.
package idempotency

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"fmt"

	"github.com/jackc/pgx/v5"

	sharederrors "github.com/aedst/server/internal/shared/errors"
)

// Store persists idempotency keys. Implemented per module under
// infrastructure/postgres, backed by that module's own schema.
type Store interface {
	Get(ctx context.Context, tx pgx.Tx, key string) (result []byte, requestHash string, found bool, err error)
	Save(ctx context.Context, tx pgx.Tx, key, requestHash string, result []byte) error
}

// HashRequest returns a hex SHA-256 digest of payload, used to detect a
// client reusing the same idempotency key for a different request.
func HashRequest(payload []byte) string {
	sum := sha256.Sum256(payload)
	return hex.EncodeToString(sum[:])
}

// Check looks up key inside tx. A miss returns (nil, false, nil): the caller
// should proceed and call Save before its transaction commits. A hit with a
// matching request hash returns (cachedResult, true, nil): the caller should
// return cachedResult without redoing the work. A hit with a different hash
// is key reuse with a different payload and returns a Conflict error.
func Check(ctx context.Context, tx pgx.Tx, store Store, key string, requestPayload []byte) (result []byte, replay bool, err error) {
	if key == "" {
		return nil, false, nil
	}

	cached, hash, found, err := store.Get(ctx, tx, key)
	if err != nil {
		return nil, false, fmt.Errorf("idempotency: get: %w", err)
	}
	if !found {
		return nil, false, nil
	}

	if hash != HashRequest(requestPayload) {
		return nil, false, sharederrors.Conflict("idempotency.Check", "idempotency key reused with a different request")
	}
	return cached, true, nil
}

// Save records key alongside requestPayload's hash and the computed result.
// A no-op when key is empty, so callers can pass an optional key through
// unconditionally.
func Save(ctx context.Context, tx pgx.Tx, store Store, key string, requestPayload, result []byte) error {
	if key == "" {
		return nil
	}
	if err := store.Save(ctx, tx, key, HashRequest(requestPayload), result); err != nil {
		return fmt.Errorf("idempotency: save: %w", err)
	}
	return nil
}
