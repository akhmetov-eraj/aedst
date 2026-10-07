package httpserver

import (
	"context"
	"encoding/json"
	"net/http"
	"sync/atomic"
	"time"
)

// Check is one named dependency probe used by /readyz.
type Check struct {
	Name string
	Fn   func(ctx context.Context) error
	// Optional marks a dependency the service degrades without instead of
	// failing (e.g. a cache or a fail-open rate limiter): a failing optional
	// check is reported under "degraded" but does not take the instance out of
	// rotation. Only checks that make correct answers impossible (the primary
	// database) should be required — otherwise a cache outage becomes a total
	// outage on every replica at once.
	Optional bool
}

// Readiness answers /readyz. It reports not-ready while any required
// dependency check fails or once SetDraining was called (graceful shutdown),
// which lets a load balancer stop routing before the listener closes.
type Readiness struct {
	checks   []Check
	draining atomic.Bool
}

func NewReadiness(checks ...Check) *Readiness {
	return &Readiness{checks: checks}
}

// SetDraining flips /readyz to 503 permanently; call at the start of shutdown.
func (r *Readiness) SetDraining() { r.draining.Store(true) }

// Draining reports whether SetDraining was called.
func (r *Readiness) Draining() bool { return r != nil && r.draining.Load() }

const readinessCheckTimeout = 2 * time.Second

// Evaluate runs every check (concurrently, so probe latency is the slowest
// dependency rather than their sum) and returns the names of failed required
// and failed optional checks.
func (r *Readiness) Evaluate(ctx context.Context) (failed, degraded []string) {
	if r == nil {
		return nil, nil
	}
	ctx, cancel := context.WithTimeout(ctx, readinessCheckTimeout)
	defer cancel()

	errs := make([]error, len(r.checks))
	done := make(chan struct{}, len(r.checks))
	for i, c := range r.checks {
		go func() {
			errs[i] = c.Fn(ctx)
			done <- struct{}{}
		}()
	}
	for range r.checks {
		<-done
	}
	for i, c := range r.checks {
		if errs[i] == nil {
			continue
		}
		if c.Optional {
			degraded = append(degraded, c.Name) // names only: never leak error detail to callers
		} else {
			failed = append(failed, c.Name)
		}
	}
	return failed, degraded
}

// Handler returns the /readyz handler. A nil Readiness is always ready.
func (r *Readiness) Handler() http.HandlerFunc {
	return func(w http.ResponseWriter, req *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		if r == nil {
			_, _ = w.Write([]byte(`{"status":"ok"}`))
			return
		}
		if r.Draining() {
			w.WriteHeader(http.StatusServiceUnavailable)
			_, _ = w.Write([]byte(`{"status":"draining"}`))
			return
		}

		failed, degraded := r.Evaluate(req.Context())
		body := map[string]any{"status": "ok"}
		switch {
		case len(failed) > 0:
			w.WriteHeader(http.StatusServiceUnavailable)
			body = map[string]any{"status": "unavailable", "failed": failed}
		case len(degraded) > 0:
			body = map[string]any{"status": "degraded", "degraded": degraded}
		}
		_ = json.NewEncoder(w).Encode(body)
	}
}
