// Package ids provides the one ID type every module uses on the wire and in
// storage, so a UUID library choice is never repeated per module.
package ids

import (
	"database/sql/driver"
	"fmt"

	"github.com/google/uuid"
)

// ID is a UUIDv7 identifier: time-ordered, so it sorts well as a Postgres
// primary key index and as a cursor for pagination.
type ID uuid.UUID

// Nil is the zero-value ID.
var Nil ID

// New generates a new time-ordered ID.
func New() ID {
	return ID(uuid.Must(uuid.NewV7()))
}

// Parse parses a canonical UUID string into an ID.
func Parse(s string) (ID, error) {
	u, err := uuid.Parse(s)
	if err != nil {
		return Nil, fmt.Errorf("ids: parse %q: %w", s, err)
	}
	return ID(u), nil
}

// MustParse is like Parse but panics on error; only use with literals.
func MustParse(s string) ID {
	id, err := Parse(s)
	if err != nil {
		panic(err)
	}
	return id
}

func (id ID) String() string {
	return uuid.UUID(id).String()
}

func (id ID) IsNil() bool {
	return id == Nil
}

func (id ID) MarshalText() ([]byte, error) {
	return []byte(id.String()), nil
}

func (id *ID) UnmarshalText(text []byte) error {
	parsed, err := Parse(string(text))
	if err != nil {
		return err
	}
	*id = parsed
	return nil
}

// Value implements driver.Valuer so pgx/database-sql can store an ID directly.
func (id ID) Value() (driver.Value, error) {
	return uuid.UUID(id).String(), nil
}

// Scan implements sql.Scanner so pgx/database-sql can read an ID directly.
func (id *ID) Scan(src any) error {
	u, ok := src.(uuid.UUID)
	if ok {
		*id = ID(u)
		return nil
	}

	switch v := src.(type) {
	case string:
		parsed, err := Parse(v)
		if err != nil {
			return err
		}
		*id = parsed
	case []byte:
		parsed, err := Parse(string(v))
		if err != nil {
			return err
		}
		*id = parsed
	default:
		return fmt.Errorf("ids: cannot scan %T into ID", src)
	}
	return nil
}
