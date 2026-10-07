// / Package outbox implements the transactional outbox on PostgreSQL.
//
// A module writes an event row (its own schema's outbox table) in the same
// database transaction as the state change that produced it (Writer.Write):
// either both commit or neither does, which closes the dual-write gap of
// "commit, then publish". Delivery happens afterwards, in batches, by a Relay
// that leases undelivered rows (FOR UPDATE SKIP LOCKED, so any number of
// server replicas can run one safely), hands each to the messaging.Publisher
// (the in-process bus by default) and marks them published.
//
// Why a relay and not one queue job per event: a per-event job doubles the
// write cost of every domain commit (row + job) and turns a burst of events
// into a burst of queue rows in the same database as the business data. The
// relay pays one INSERT at commit time and amortizes delivery over batches;
// its cost is a poll interval of latency (default 500ms), which integration
// events tolerate by definition.
//
// Delivery is at-least-once: retries use exponential backoff, rows that
// exhaust their attempts are marked poisoned and kept for inspection, and
// consumers dedupe through platform/inbox. The relay exports lag metrics
// (see metrics.go) so a stalled consumer or a growing backlog is visible
// before it is an incident.
//
// This package never depends on a wire format: the event payload is opaque
// bytes chosen by the producing module.
package outbox

import (
	"context"
	"time"

	"go.opentelemetry.io/otel"
	"go.opentelemetry.io/otel/propagation"

	"github.com/aedst/server/internal/shared/ids"
	sharedtime "github.com/aedst/server/internal/shared/time"
)

// Row is one outbox entry: an event that has committed to the database and
// is owed delivery to subscribers.
type Row struct {
	EventID      ids.ID
	EventType    string
	EventVersion uint32
	OccurredAt   time.Time
	PartitionKey ids.ID
	Traceparent  string
	Payload      []byte // the event body, already serialized by the caller
	PublishedAt  *time.Time
	Attempts     int
	PoisonedAt   *time.Time
}

// NewRow builds an undelivered Row for eventType with the given payload. The
// W3C traceparent of ctx is captured so the asynchronous delivery stays in
// the originating request's trace.
func NewRow(ctx context.Context, clock sharedtime.Clock, eventType string, eventVersion uint32, partitionKey ids.ID, payload []byte) Row {
	return Row{
		EventID:      ids.New(),
		EventType:    eventType,
		EventVersion: eventVersion,
		OccurredAt:   clock.Now(),
		PartitionKey: partitionKey,
		Traceparent:  traceparentFromContext(ctx),
		Payload:      payload,
	}
}

func traceparentFromContext(ctx context.Context) string {
	carrier := propagation.MapCarrier{}
	otel.GetTextMapPropagator().Inject(ctx, carrier)
	return carrier.Get("traceparent")
}

func contextWithTraceparent(ctx context.Context, traceparent string) context.Context {
	if traceparent == "" {
		return ctx
	}
	return otel.GetTextMapPropagator().Extract(ctx, propagation.MapCarrier{"traceparent": traceparent})
}
