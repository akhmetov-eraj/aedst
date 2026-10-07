package valkey

import (
	"context"
	"fmt"
	"time"

	"github.com/go-redis/redis_rate/v10"
	"github.com/redis/go-redis/v9"
)

// RateLimiter is a cluster-shared GCRA rate limiter (redis_rate): every
// replica shares the same budget for a key instead of each enforcing its
// own independent limit.
type RateLimiter struct {
	limiter *redis_rate.Limiter
}

// NewRateLimiter builds a RateLimiter on an existing client.
func NewRateLimiter(client *redis.Client) *RateLimiter {
	return &RateLimiter{limiter: redis_rate.NewLimiter(client)}
}

// Allow reports whether one more request is permitted for key under
// limit-per-period. It fails open: on a Valkey error it returns true with the
// error, because a limiter that takes the service down when its backing
// store is unavailable is worse than briefly not limiting. Callers should
// log the error.
func (l *RateLimiter) Allow(ctx context.Context, key string, limit int, period time.Duration) (bool, error) {
	res, err := l.limiter.Allow(ctx, key, redis_rate.Limit{Rate: limit, Burst: limit, Period: period})
	if err != nil {
		return true, fmt.Errorf("valkey: rate limit %s: %w", key, err)
	}
	return res.Allowed > 0, nil
}
