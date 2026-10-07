package ratelimit_test

import (
	"context"
	"testing"
	"time"

	chimw "github.com/go-chi/chi/v5/middleware"

	"github.com/aedst/server/internal/modules/auth/infrastructure/ratelimit"
)

// budgets is an exact-count limiter: each key gets `limit` calls, then is
// refused. It records the limit each key was checked against.
type budgets struct {
	used   map[string]int
	limits map[string]int
}

func newBudgets() *budgets { return &budgets{used: map[string]int{}, limits: map[string]int{}} }

func (b *budgets) Allow(_ context.Context, key string, limit int, _ time.Duration) (bool, error) {
	b.limits[key] = limit
	b.used[key]++
	return b.used[key] <= limit, nil
}

func fromAddress(ip string) context.Context {
	// chi stores the client IP under an unexported key, set by its
	// ClientIP middleware; go through the middleware to build a real context.
	var ctx context.Context
	h := chimw.ClientIPFromRemoteAddr(handlerFunc(func(c context.Context) { ctx = c }))
	req, _ := newRequest(ip)
	h.ServeHTTP(nil, req)
	return ctx
}

// An attacker hammering one account from one address gets locked out — but the
// account's real owner, coming from another address, still gets in. Keying the
// budget on the account alone would let anyone lock anyone out.
func TestLoginLimiter_AttackerCannotLockOutTheOwner(t *testing.T) {
	limiter := ratelimit.NewLoginLimiter(newBudgets(), 5)
	attacker, owner := fromAddress("203.0.113.9:5000"), fromAddress("198.51.100.7:6000")

	for i := 0; i < 5; i++ {
		if ok, err := limiter.Allow(attacker, "victim@example.com"); err != nil || !ok {
			t.Fatalf("attempt %d from the attacker was refused early: %v, %v", i+1, ok, err)
		}
	}
	if ok, _ := limiter.Allow(attacker, "victim@example.com"); ok {
		t.Fatal("the attacker's sixth attempt from one address was allowed")
	}
	if ok, err := limiter.Allow(owner, "victim@example.com"); err != nil || !ok {
		t.Fatalf("the owner was locked out by someone else's guesses: %v, %v", ok, err)
	}
}

// A distributed guess that rotates addresses is stopped by the per-account
// ceiling, which is deliberately much higher than the per-address budget.
func TestLoginLimiter_AccountCeilingStopsDistributedGuessing(t *testing.T) {
	b := newBudgets()
	limiter := ratelimit.NewLoginLimiter(b, 2) // ceiling: 20 attempts across all addresses
	allowed := 0
	for i := 0; i < 100; i++ {
		ip := "192.0.2." + itoa(i%250+1) + ":1"
		if ok, _ := limiter.Allow(fromAddress(ip), "victim@example.com"); ok {
			allowed++
		}
	}
	if allowed != 20 {
		t.Fatalf("allowed %d attempts across many addresses, want the ceiling of 20", allowed)
	}
}

func TestLoginLimiter_KeysNeverContainTheEmail(t *testing.T) {
	b := newBudgets()
	_, _ = ratelimit.NewLoginLimiter(b, 5).Allow(context.Background(), "victim@example.com")
	for key := range b.used {
		if contains(key, "victim") || contains(key, "example.com") {
			t.Errorf("limiter key %q leaks the account email", key)
		}
	}
	if len(b.used) != 2 {
		t.Errorf("limiter keys = %v, want one per-address key and one per-account key", b.used)
	}
}
