// Package valkey wires the shared Valkey client. Valkey is not the primary
// database: it backs cache and rate limiting only (see
// docs/content/architecture.md). It is deliberately NOT used for locks
// (use internal/platform/lock, PostgreSQL advisory locks) or for messaging
// (use the outbox/inbox + River, see internal/platform/messaging).
//
// The client is github.com/redis/go-redis/v9: Valkey speaks the same RESP
// protocol, so no separate client library is needed. Keep every go-redis
// import inside this package.
//
// Valkey is an optional dependency by design: nothing correct depends on it,
// so the client neither fails process startup nor takes a replica out of
// rotation when it is unreachable (see cmd/server and httpserver.Check's
// Optional flag). It reconnects on its own once Valkey is back.
package valkey

import (
	"context"
	"fmt"
	"time"

	"github.com/redis/go-redis/v9"
)

type Config struct {
	Addr     string
	Password string
	DB       int
}

// NewClient builds the client. It does not connect: connections are dialed
// lazily and re-dialed after failures.
func NewClient(cfg Config) *redis.Client {
	return redis.NewClient(&redis.Options{
		Addr:         cfg.Addr,
		Password:     cfg.Password,
		DB:           cfg.DB,
		DialTimeout:  time.Second,
		ReadTimeout:  500 * time.Millisecond,
		WriteTimeout: 500 * time.Millisecond,
		MaxRetries:   1,
	})
}

// Ping reports whether Valkey answers, for readiness reporting and a startup
// log line.
func Ping(ctx context.Context, client *redis.Client) error {
	if err := client.Ping(ctx).Err(); err != nil {
		return fmt.Errorf("valkey: ping: %w", err)
	}
	return nil
}
