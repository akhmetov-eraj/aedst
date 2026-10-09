// Package ratelimit implements application.LoginLimiter on the shared
// Valkey rate limiter, so the budget is enforced across every replica.
package ratelimit

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"time"

	"github.com/aedst/server/internal/platform/clientip"
)

// accountCeilingFactor scales the per-(account, address) budget up to the
// per-account ceiling.
const accountCeilingFactor = 10

// Limiter is satisfied by *valkey.RateLimiter.
type Limiter interface {
	Allow(ctx context.Context, key string, limit int, period time.Duration) (bool, error)
}

type LoginLimiter struct {
	limiter   Limiter
	perMinute int
}

func NewLoginLimiter(limiter Limiter, perMinute int) *LoginLimiter {
	return &LoginLimiter{limiter: limiter, perMinute: perMinute}
}

// Allow throttles password attempts on two independent budgets, and both must
// have room:
//
//   - per (account, client address): perMinute. An attacker guessing one
//     account's password from one address is stopped here, and — the point of
//     keying on the address too — cannot exhaust the victim's budget from
//     that address, so they cannot lock the real owner out from elsewhere.
//   - per account: accountCeilingFactor x perMinute across all addresses, the
//     backstop against a distributed guess that rotates addresses.
//
// Keys use a digest so account emails never appear in Valkey.
func (l *LoginLimiter) Allow(ctx context.Context, email string) (bool, error) {
	account := digest(email)
	client := clientip.FromContext(ctx)
	if client == "" {
		client = "unknown"
	}

	allowed, err := l.limiter.Allow(ctx, "auth:login:"+account+":"+digest(client), l.perMinute, time.Minute)
	if err != nil || !allowed {
		return allowed, err
	}
	return l.limiter.Allow(ctx, "auth:login:"+account, l.perMinute*accountCeilingFactor, time.Minute)
}

func digest(s string) string {
	sum := sha256.Sum256([]byte(s))
	return hex.EncodeToString(sum[:16])
}
