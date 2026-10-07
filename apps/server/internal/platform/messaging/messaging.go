// Package messaging is the in-process event bus that sits at the end of the
// transactional outbox pipeline:
//
//	domain change + outbox row (one Postgres tx)
//	    -> outbox.Relay (batched, lease-based, any replica)
//	    -> Publisher.Publish  (this package)
//	    -> subscriber handlers (each guarded by internal/platform/inbox)
//
// There is exactly one asynchronous mechanism family in this codebase:
// Postgres outbox + inbox (+ River for deferred jobs). Retries, backoff and
// poisoning are the relay's; exactly-once *effect* comes from the inbox. No
// external broker is required.
//
// Publisher is the seam where an external broker (NATS JetStream, Kafka, ...)
// can be plugged in later — implement it in a new adapter and pass that to
// the outbox relay — but do NOT do so until an in-process bus is proven
// insufficient (cross-service fan-out, independent scaling of consumers).
// Until then Bus is the only implementation.
package messaging

import (
	"context"
	"errors"
	"fmt"
	"sync"
	"time"

	"github.com/aedst/server/internal/shared/ids"
)

// Event is one integration event as delivered to subscribers. ID is the
// outbox row id and is stable across redeliveries — consumers dedupe on it.
type Event struct {
	ID           ids.ID
	Type         string
	Version      uint32
	OccurredAt   time.Time
	PartitionKey ids.ID
	// Payload is the event body, already serialized by the producing module
	// (JSON today). Consumers decode it themselves.
	Payload []byte
}

// Handler processes one event. It must be idempotent: the outbox relay
// delivers at-least-once (a failed or crashed delivery is retried). Wrap handlers that touch the database with
// internal/platform/inbox.Consumer to get effectively-once processing.
type Handler func(ctx context.Context, event Event) error

// Publisher delivers an event to its subscribers. Returning an error makes
// the outbox relay retry the event with backoff (and poison it after too many
// attempts), so implementations must be safe to call again with the same
// Event.ID.
type Publisher interface {
	Publish(ctx context.Context, event Event) error
}

// Bus is the in-process Publisher: it fans an event out to every handler
// subscribed to its type. Register subscriptions at startup (cmd/server)
// before the outbox relays start.
type Bus struct {
	mu       sync.RWMutex
	handlers map[string][]namedHandler
}

type namedHandler struct {
	name string
	fn   Handler
}

var _ Publisher = (*Bus)(nil)

func NewBus() *Bus {
	return &Bus{handlers: make(map[string][]namedHandler)}
}

// Subscribe registers handler under a unique name (for error reporting) for
// eventType, e.g. "tasks.v1.TaskCompleted".
func (b *Bus) Subscribe(eventType, name string, handler Handler) {
	b.mu.Lock()
	defer b.mu.Unlock()
	b.handlers[eventType] = append(b.handlers[eventType], namedHandler{name: name, fn: handler})
}

// Publish runs every subscriber for event.Type. All handlers run even if one
// fails (so a broken subscriber cannot starve the others); the failures are
// joined into the returned error so the job retries. An event with no
// subscribers is a successful no-op.
func (b *Bus) Publish(ctx context.Context, event Event) error {
	b.mu.RLock()
	subs := b.handlers[event.Type]
	b.mu.RUnlock()

	var errs []error
	for _, sub := range subs {
		if err := sub.fn(ctx, event); err != nil {
			errs = append(errs, fmt.Errorf("subscriber %s: %w", sub.name, err))
		}
	}
	return errors.Join(errs...)
}
