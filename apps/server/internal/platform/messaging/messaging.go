// Package messaging wires NATS JetStream for domain/integration events (see
// docs/events.md for subject naming, versioning and idempotency conventions).
// Modules never talk to the nats.Conn directly — they depend on the
// Publisher/Subscriber interfaces below, which keeps event publishing testable
// without a running broker.
package messaging

import (
	"context"
	"fmt"

	"github.com/google/uuid"
	"github.com/nats-io/nats.go"
	"github.com/nats-io/nats.go/jetstream"
	"go.opentelemetry.io/otel/trace"
	"google.golang.org/protobuf/proto"
)

const (
	// HeaderEventID carries the idempotency key every consumer must dedupe on.
	HeaderEventID = "Aedst-Event-Id"
	// HeaderTraceID propagates the originating request's trace for
	// correlation across the async boundary (see docs/events.md).
	HeaderTraceID = "Aedst-Trace-Id"
)

// Publisher publishes a domain event onto a subject. Implemented by *Client.
type Publisher interface {
	Publish(ctx context.Context, subject string, event proto.Message) error
}

// Client wraps a NATS connection and its JetStream context.
type Client struct {
	conn *nats.Conn
	js   jetstream.JetStream
}

func Connect(url string) (*Client, error) {
	conn, err := nats.Connect(url,
		nats.RetryOnFailedConnect(true),
		nats.MaxReconnects(-1),
	)
	if err != nil {
		return nil, fmt.Errorf("messaging: connect: %w", err)
	}

	js, err := jetstream.New(conn)
	if err != nil {
		conn.Close()
		return nil, fmt.Errorf("messaging: jetstream: %w", err)
	}

	return &Client{conn: conn, js: js}, nil
}

func (c *Client) Close() {
	c.conn.Close()
}

// EnsureStream creates the named stream if it doesn't exist yet. Called once
// per stream at startup by cmd/server; modules declare the subjects they
// publish under their own stream (e.g. "AEDST_PROFILE" for
// "aedst.profile.>").
func (c *Client) EnsureStream(ctx context.Context, name string, subjects []string) error {
	_, err := c.js.CreateOrUpdateStream(ctx, jetstream.StreamConfig{
		Name:     name,
		Subjects: subjects,
		Storage:  jetstream.FileStorage,
	})
	if err != nil {
		return fmt.Errorf("messaging: ensure stream %s: %w", name, err)
	}
	return nil
}

// Publish marshals event and publishes it to subject with an idempotency
// key and the current trace ID attached as headers.
func (c *Client) Publish(ctx context.Context, subject string, event proto.Message) error {
	payload, err := proto.Marshal(event)
	if err != nil {
		return fmt.Errorf("messaging: marshal event: %w", err)
	}

	msg := nats.NewMsg(subject)
	msg.Data = payload
	msg.Header.Set(HeaderEventID, uuid.Must(uuid.NewV7()).String())
	if sc := trace.SpanContextFromContext(ctx); sc.HasTraceID() {
		msg.Header.Set(HeaderTraceID, sc.TraceID().String())
	}

	if _, err := c.js.PublishMsg(ctx, msg); err != nil {
		return fmt.Errorf("messaging: publish %s: %w", subject, err)
	}
	return nil
}

// HandlerFunc processes one delivered message. Returning an error leaves the
// message unacked for JetStream redelivery; handlers must be idempotent (see
// docs/events.md).
type HandlerFunc func(ctx context.Context, msg jetstream.Msg) error

// Subscribe creates (or attaches to) a durable consumer and processes
// messages with handler until ctx is cancelled.
func (c *Client) Subscribe(ctx context.Context, stream, durable, filterSubject string, handler HandlerFunc) error {
	cons, err := c.js.CreateOrUpdateConsumer(ctx, stream, jetstream.ConsumerConfig{
		Durable:       durable,
		FilterSubject: filterSubject,
		AckPolicy:     jetstream.AckExplicitPolicy,
	})
	if err != nil {
		return fmt.Errorf("messaging: create consumer %s: %w", durable, err)
	}

	_, err = cons.Consume(func(msg jetstream.Msg) {
		if err := handler(ctx, msg); err != nil {
			_ = msg.Nak()
			return
		}
		_ = msg.Ack()
	})
	if err != nil {
		return fmt.Errorf("messaging: consume %s: %w", durable, err)
	}

	<-ctx.Done()
	return nil
}
