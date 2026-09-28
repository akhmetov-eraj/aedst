package redis

import (
	"context"
	"fmt"
	"time"

	"github.com/go-redis/redis_rate/v10"
	"github.com/go-redsync/redsync/v4"
	"github.com/go-redsync/redsync/v4/redis/goredis/v9"
	"github.com/redis/go-redis/v9"
)

// Locker provides cluster-wide mutual exclusion via Redlock, for work that
// must run on at most one replica at a time (e.g. a periodic sweep). Not for
// per-request locking — use idempotency keys for that instead (see
// internal/platform/idempotency).
type Locker struct {
	rs *redsync.Redsync
}

// NewLocker builds a Locker on top of an existing Redis client.
func NewLocker(client *redis.Client) *Locker {
	pool := goredis.NewPool(client)
	return &Locker{rs: redsync.New(pool)}
}

// Lock blocks until name is acquired or ctx is done, then returns a release
// function the caller must call (typically via defer) to release it before
// ttl expires on its own.
func (l *Locker) Lock(ctx context.Context, name string, ttl time.Duration) (release func(), err error) {
	mutex := l.rs.NewMutex(name, redsync.WithExpiry(ttl))
	if err := mutex.LockContext(ctx); err != nil {
		return nil, fmt.Errorf("redis: lock %s: %w", name, err)
	}
	return func() { _, _ = mutex.UnlockContext(ctx) }, nil
}

// RedisLimiter is a cluster-shared sliding-window rate limiter: every
// replica of this service shares the same budget for key, instead of each
// replica enforcing its own independent limit.
type RedisLimiter struct {
	limiter *redis_rate.Limiter
	key     string
	limit   redis_rate.Limit
}

// NewRedisLimiter builds a RedisLimiter allowing up to rps requests per
// second, shared across every caller of Allow for the same key.
func NewRedisLimiter(client *redis.Client, key string, rps float64) *RedisLimiter {
	return &RedisLimiter{
		limiter: redis_rate.NewLimiter(client),
		key:     key,
		limit:   redis_rate.PerSecond(int(rps)),
	}
}

// Allow reports whether one more request is permitted right now. On a Redis
// error, Allow fails open (returns true) — a rate limiter that itself takes
// the service down when its backing store is unavailable is a worse outcome
// than briefly not being rate limited; the error is still returned so the
// caller can log it.
func (l *RedisLimiter) Allow(ctx context.Context) (bool, error) {
	result, err := l.limiter.Allow(ctx, l.key, l.limit)
	if err != nil {
		return true, fmt.Errorf("redis: rate limit %s: %w", l.key, err)
	}
	return result.Allowed > 0, nil
}
