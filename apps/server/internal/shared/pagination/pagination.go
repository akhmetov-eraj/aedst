// Package pagination provides cursor-based pagination primitives, used
// instead of offset pagination for any collection that can grow large
// (messages, feeds, listings) — see docs/architecture.md.
package pagination

import (
	"encoding/base64"
	"fmt"
	"time"
)

const (
	DefaultLimit = 20
	MaxLimit     = 100
)

// Cursor encodes the position to resume from. Modules embed whatever fields
// their ordering needs (typically a timestamp + tie-breaking ID); this type
// only standardizes the opaque wire encoding.
type Cursor struct {
	CreatedAt time.Time
	ID        string
}

// Encode produces the opaque cursor string returned to clients.
func (c Cursor) Encode() string {
	raw := fmt.Sprintf("%d:%s", c.CreatedAt.UnixNano(), c.ID)
	return base64.RawURLEncoding.EncodeToString([]byte(raw))
}

// DecodeCursor parses a cursor string previously produced by Encode.
func DecodeCursor(s string) (Cursor, error) {
	if s == "" {
		return Cursor{}, nil
	}
	raw, err := base64.RawURLEncoding.DecodeString(s)
	if err != nil {
		return Cursor{}, fmt.Errorf("pagination: invalid cursor: %w", err)
	}
	var nanos int64
	var id string
	if _, err := fmt.Sscanf(string(raw), "%d:%s", &nanos, &id); err != nil {
		return Cursor{}, fmt.Errorf("pagination: invalid cursor: %w", err)
	}
	return Cursor{CreatedAt: time.Unix(0, nanos), ID: id}, nil
}

// ClampLimit normalizes a client-supplied page size to a safe range.
func ClampLimit(limit int32) int32 {
	if limit <= 0 {
		return DefaultLimit
	}
	if limit > MaxLimit {
		return MaxLimit
	}
	return limit
}

// Page wraps a page of results with the cursor to request the next one.
type Page[T any] struct {
	Items      []T
	NextCursor string // empty when there are no more results
}
