// Package httpserver builds the chi router and the small, deliberately short
// middleware stack every request goes through (CORS, request ID, logging,
// panic recovery, timeout) plus the ConnectRPC interceptors (auth, rate
// limiting). Tracing and HTTP metrics come from OpenTelemetry
// (otelhttp/otelconnect, see Instrument), not hand-rolled middleware.
// Modules mount ConnectRPC handlers onto the router this package builds;
// they never construct their own HTTP server.
package httpserver

import (
	"log/slog"
	"net/http"
	"regexp"
	"runtime/debug"
	"time"
	"uuid"

	"github.com/go-chi/chi/v5"
	chimw "github.com/go-chi/chi/v5/middleware"
	"github.com/go-chi/cors"
	"go.opentelemetry.io/contrib/instrumentation/net/http/otelhttp"

	"github.com/aedst/server/internal/platform/observability"
)

// Options configures the router's middleware stack.
type Options struct {
	Logger         *slog.Logger
	RequestTimeout time.Duration
	// AllowedOrigins are the browser origins permitted to call the API
	// cross-origin (see config.HTTPConfig.AllowedOrigins). A Connect-ES web
	// client (e.g. apps/web) is on a different origin in local dev, so this
	// must be set or every browser call fails CORS preflight.
	AllowedOrigins []string
	// TrustedProxyHops is the exact number of trusted reverse proxies in front
	// of this server; the client IP is then read from X-Forwarded-For. 0 uses
	// the socket peer address. (chi's deprecated RealIP is deliberately not
	// used: it is spoofable.)
	TrustedProxyHops int
	// Readiness backs /readyz. Optional; without it /readyz always reports
	// ready.
	Readiness *Readiness
	// MaxInFlight caps concurrently processing requests (probes excluded); the
	// excess is shed immediately with 429 + Retry-After instead of queueing
	// until the whole instance drowns. 0 disables shedding.
	MaxInFlight int
	// MaxBodyBytes caps every request body at the HTTP layer
	// (http.MaxBytesReader), before any decoding. Per-service message limits
	// (connect.WithReadMaxBytes) are applied on top by the RPC mounter.
	// 0 disables.
	MaxBodyBytes int64
	// HSTSMaxAge, when positive, sends Strict-Transport-Security on requests
	// that arrived over HTTPS (directly, or per X-Forwarded-Proto from a
	// trusted proxy when TrustedProxyHops > 0).
	HSTSMaxAge time.Duration
	// StreamingProcedures are exempt from RequestTimeout and from the server's
	// WriteTimeout (cleared per request through http.ResponseController), so
	// a long-lived stream is not cut off by limits sized for unary calls.
	StreamingProcedures map[string]bool
}

// NewRouter builds the base chi router with the standard middleware stack
// applied.
func NewRouter(opts Options) *chi.Mux {
	r := chi.NewRouter()

	r.Use(corsMiddleware(opts.AllowedOrigins))
	r.Use(securityHeaders(opts.HSTSMaxAge, opts.TrustedProxyHops > 0))
	r.Use(requestIDMiddleware)
	if opts.TrustedProxyHops > 0 {
		r.Use(chimw.ClientIPFromXFFTrustedProxies(opts.TrustedProxyHops))
	} else {
		r.Use(chimw.ClientIPFromRemoteAddr)
	}
	r.Use(loggingMiddleware(opts.Logger))
	r.Use(recoveryMiddleware(opts.Logger))
	if opts.MaxInFlight > 0 {
		r.Use(skipProbes(chimw.ThrottleWithOpts(chimw.ThrottleOpts{
			Limit:        opts.MaxInFlight,
			RetryAfterFn: func(bool) time.Duration { return time.Second },
		})))
	}
	if opts.MaxBodyBytes > 0 {
		r.Use(maxBodyBytes(opts.MaxBodyBytes))
	}
	r.Use(streamingDeadlines(opts.StreamingProcedures, opts.Logger))
	if opts.RequestTimeout > 0 {
		r.Use(skipPaths(opts.StreamingProcedures, chimw.Timeout(opts.RequestTimeout)))
	}

	// Liveness: the process is up and serving, nothing more. No dependency
	// checks here, so a database outage does not make an orchestrator
	// restart otherwise-healthy instances.
	r.Get("/livez", func(w http.ResponseWriter, _ *http.Request) {
		w.WriteHeader(http.StatusOK)
	})
	// Readiness: dependencies reachable and not draining; gates traffic.
	r.Get("/readyz", opts.Readiness.Handler())

	return r
}

// Instrument wraps handler with OpenTelemetry HTTP server tracing and
// metrics. It must be the OUTERMOST wrapper so every middleware (and every
// log line) runs inside the request span. Probe endpoints are not traced.
func Instrument(handler http.Handler, serviceName string) http.Handler {
	return otelhttp.NewHandler(handler, serviceName,
		otelhttp.WithFilter(func(r *http.Request) bool { return r.URL.Path != "/livez" && r.URL.Path != "/readyz" }),
		// Span name is the method only: paths of unmatched requests are
		// attacker-controlled and would explode cardinality. RPC spans get
		// proper rpc.service/rpc.method names from otelconnect.
		otelhttp.WithSpanNameFormatter(func(_ string, r *http.Request) string { return r.Method }),
	)
}

// corsMiddleware allows a Connect-ES browser client on allowedOrigins to call
// this API cross-origin. Connect's unary protocol runs over plain HTTP
// POST+JSON (or protobuf), so it needs the usual CORS headers plus the
// Connect-specific ones a browser client sends; auth is a bearer token, not a
// cookie, so credentials aren't required.
func corsMiddleware(allowedOrigins []string) func(http.Handler) http.Handler {
	return cors.Handler(cors.Options{
		AllowedOrigins: allowedOrigins,
		AllowedMethods: []string{http.MethodGet, http.MethodPost, http.MethodOptions},
		AllowedHeaders: []string{
			"Content-Type",
			"Authorization",
			"Connect-Protocol-Version",
			"Connect-Timeout-Ms",
			"Connect-Content-Encoding",
			"Connect-Accept-Encoding",
			"Idempotency-Key",
			"X-Aedst-Client",
			"X-Request-Id",
			"X-Grpc-Web",
		},
		ExposedHeaders: []string{
			"Connect-Content-Encoding",
			"Connect-Accept-Encoding",
			"X-Request-Id",
		},
		// Bearer tokens only: browsers must never attach cookies or HTTP auth.
		AllowCredentials: false,
		MaxAge:           3600,
	})
}

// validRequestID accepts the shapes real clients send (UUIDs, ULIDs, short
// tokens) and rejects anything that could inject into logs or headers.
var validRequestID = regexp.MustCompile(`^[A-Za-z0-9._-]{1,64}$`)

func requestIDMiddleware(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		id := r.Header.Get("X-Request-Id")
		if !validRequestID.MatchString(id) {
			id = uuid.NewV7().String()
		}
		w.Header().Set("X-Request-Id", id)
		ctx := observability.WithRequestID(r.Context(), id)
		next.ServeHTTP(w, r.WithContext(ctx))
	})
}

func recoveryMiddleware(logger *slog.Logger) func(http.Handler) http.Handler {
	return func(next http.Handler) http.Handler {
		return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			defer func() { //nolint:contextcheck // recover handler runs in the request goroutine; r.Context() is used below
				rec := recover()
				if rec == nil {
					return
				}
				if rec == http.ErrAbortHandler { //nolint:errorlint // sentinel panic value, not an error chain
					panic(rec) // net/http's own "abort this response" signal
				}
				logger.ErrorContext(r.Context(), "panic recovered",
					slog.Any("panic", rec),
					slog.String("stack", string(debug.Stack())),
				)
				http.Error(w, "internal server error", http.StatusInternalServerError)
			}()
			next.ServeHTTP(w, r)
		})
	}
}

func loggingMiddleware(logger *slog.Logger) func(http.Handler) http.Handler {
	return func(next http.Handler) http.Handler {
		return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			start := time.Now()
			ww := chimw.NewWrapResponseWriter(w, r.ProtoMajor)
			next.ServeHTTP(ww, r)
			if isProbe(r.URL.Path) {
				return // probes fire every few seconds; not worth a log line each
			}
			logger.InfoContext(r.Context(), "request",
				slog.String("method", r.Method),
				slog.String("path", r.URL.Path),
				slog.Int("status", ww.Status()),
				slog.Duration("duration", time.Since(start)),
			)
		})
	}
}

func isProbe(path string) bool { return path == "/livez" || path == "/readyz" }

// skipProbes applies mw to everything except the health probes: a saturated
// instance must still answer /livez and /readyz, or the orchestrator would
// restart (or stop routing to) exactly the instance that is shedding load
// correctly.
func skipProbes(mw func(http.Handler) http.Handler) func(http.Handler) http.Handler {
	return func(next http.Handler) http.Handler {
		guarded := mw(next)
		return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			if isProbe(r.URL.Path) {
				next.ServeHTTP(w, r)
				return
			}
			guarded.ServeHTTP(w, r)
		})
	}
}
