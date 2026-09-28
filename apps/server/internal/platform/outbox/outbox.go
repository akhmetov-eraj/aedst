// Package outbox implements the transactional outbox pattern: a module
// writes an event row in the same database transaction as the state change
// that produced it, and a background Forwarder publishes rows to the message
// bus asynchronously. This closes the dual-write gap that a direct
// "commit, then publish" flow has — a crash between the two leaves the event
// unpublished with no record it was ever owed (see docs/events.md).
//
// platform/outbox never depends on a wire format (protobuf, JSON, ...): the
// caller supplies an EnvelopeMarshaler when constructing a Forwarder, keeping
// this package usable by any module regardless of how it encodes events.
package outbox

import (
	"encoding/json"
	"fmt"
	"time"

	"github.com/aedst/server/internal/shared/ids"
	sharedtime "github.com/aedst/server/internal/shared/time"
)

// Row is one outbox entry: an event that has committed to the database and
// is owed a publish to the bus.
type Row struct {
	EventID      ids.ID
	EventType    string
	EventVersion uint32
	OccurredAt   time.Time
	PartitionKey ids.ID
	Traceparent  string
	Payload      []byte // the inner event, already serialized by the caller
	PublishedAt  *time.Time
	Attempts     int
	NextRetryAt  *time.Time
}

// NewRow builds an unpublished Row for eventType with the given payload.
func NewRow(clock sharedtime.Clock, eventType string, eventVersion uint32, partitionKey ids.ID, traceparent string, payload []byte) Row {
	return Row{
		EventID:      ids.New(),
		EventType:    eventType,
		EventVersion: eventVersion,
		OccurredAt:   clock.Now(),
		PartitionKey: partitionKey,
		Traceparent:  traceparent,
		Payload:      payload,
	}
}

// EnvelopeMarshaler wraps a Row's metadata and payload into the bytes that
// get published to the bus. Supplied by the caller so this package stays
// independent of any particular wire format.
type EnvelopeMarshaler func(row Row) ([]byte, error)

// Envelope is the default, JSON-based wrapper used by JSONEnvelope. Modules
// that need a protobuf contract (e.g. a shared common.v1.Envelope) supply
// their own EnvelopeMarshaler instead of this one.
type Envelope struct {
	EventID      string          `json:"event_id"`
	EventType    string          `json:"event_type"`
	EventVersion uint32          `json:"event_version"`
	OccurredAt   time.Time       `json:"occurred_at"`
	PartitionKey string          `json:"partition_key"`
	Traceparent  string          `json:"traceparent,omitempty"`
	Payload      json.RawMessage `json:"payload"`
}

// JSONEnvelope is a ready-to-use EnvelopeMarshaler for modules that don't
// (yet) have a protobuf event contract. row.Payload must already be valid
// JSON (e.g. produced by json.Marshal of the module's event struct).
func JSONEnvelope(row Row) ([]byte, error) {
	envelope := Envelope{
		EventID:      row.EventID.String(),
		EventType:    row.EventType,
		EventVersion: row.EventVersion,
		OccurredAt:   row.OccurredAt,
		PartitionKey: row.PartitionKey.String(),
		Traceparent:  row.Traceparent,
		Payload:      row.Payload,
	}
	data, err := json.Marshal(envelope)
	if err != nil {
		return nil, fmt.Errorf("outbox: marshal envelope: %w", err)
	}
	return data, nil
}
