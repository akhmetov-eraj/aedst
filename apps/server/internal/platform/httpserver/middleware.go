// Package httpserver builds the chi router and the small, deliberately short
// middleware stack every request goes through (request ID, recovery, logging,
// tracing, metrics, auth, rate limiting, timeout — see project brief section
// 32). Modules mount ConnectRPC handlers onto the router this package builds;
// they never construct their own HTTP server.
package httpserver

import (
	"log/slog"
	"net/http"
	"time"

	"github.com/go-chi/chi/v5"
	chimw "github.com/go-chi/chi/v5/middleware"
	"github.com/go-chi/cors"
	"go.opentelemetry.io/otel"
	"go.opentelemetry.io/otel/propagation"
	"go.opentelemetry.io/otel/trace"
	"uuid"

	platformauth "github.com/aedst/server/internal/platform/auth"
	"github.com/aedst/server/internal/platform/observability"
)

// Options configures the router's middleware stack.
type Options struct {
	Logger         *slog.Logger
	Tracer         trace.Tracer
	Metrics        *observability.Metrics
	Tokens         *platformauth.TokenService
	RequestTimeout time.Duration
	// AllowedOrigins are the browser origins permitted to call the API
	// cross-origin (see config.HTTPConfig.AllowedOrigins). A Connect-ES web
	// client (e.g. apps/web) is on a different origin in local dev, so this
	// must be set or every browser call fails CORS preflight.
	AllowedOrigins []string
}

// NewRouter builds the base chi router with the standard middleware stack
// applied. Public (unauthenticated) and authenticated route groups are
// obtained via Public() and Authenticated() below.
func NewRouter(opts Options) *chi.Mux {
	r := chi.NewRouter()

	r.Use(corsMiddleware(opts.AllowedOrigins))
	r.Use(requestIDMiddleware)
	r.Use(recoveryMiddleware(opts.Logger))
	r.Use(tracingMiddleware(opts.Tracer))
	r.Use(loggingMiddleware(opts.Logger))
	if opts.Metrics != nil {
		r.Use(metricsMiddleware(opts.Metrics))
	}
	if opts.RequestTimeout > 0 {
		r.Use(chimw.Timeout(opts.RequestTimeout))
	}

	return r
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
			"X-Request-Id",
			"X-Grpc-Web",
		},
		ExposedHeaders: []string{
			"Connect-Content-Encoding",
			"Connect-Accept-Encoding",
			"X-Request-Id",
		},
		MaxAge: 3600,
	})
}

func requestIDMiddleware(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		id := r.Header.Get("X-Request-Id")
		if id == "" {
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
			defer func() {
				if rec := recover(); rec != nil {
					logger.ErrorContext(r.Context(), "panic recovered", slog.Any("panic", rec))
					http.Error(w, "internal server error", http.StatusInternalServerError)
				}
			}()
			next.ServeHTTP(w, r)
		})
	}
}

func tracingMiddleware(tracer trace.Tracer) func(http.Handler) http.Handler {
	propagator := otel.GetTextMapPropagator()
	return func(next http.Handler) http.Handler {
		return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			ctx := propagator.Extract(r.Context(), propagation.HeaderCarrier(r.Header))
			ctx, span := tracer.Start(ctx, r.Method+" "+r.URL.Path)
			defer span.End()
			ctx = observability.WithTraceID(ctx, observability.TraceIDFromContext(ctx))
			next.ServeHTTP(w, r.WithContext(ctx))
		})
	}
}

func loggingMiddleware(logger *slog.Logger) func(http.Handler) http.Handler {
	return func(next http.Handler) http.Handler {
		return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			start := time.Now()
			ww := chimw.NewWrapResponseWriter(w, r.ProtoMajor)
			next.ServeHTTP(ww, r)
			logger.InfoContext(r.Context(), "request",
				slog.String("method", r.Method),
				slog.String("path", r.URL.Path),
				slog.Int("status", ww.Status()),
				slog.Duration("duration", time.Since(start)),
			)
		})
	}
}

func metricsMiddleware(metrics *observability.Metrics) func(http.Handler) http.Handler {
	return func(next http.Handler) http.Handler {
		return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			start := time.Now()
			ww := chimw.NewWrapResponseWriter(w, r.ProtoMajor)
			next.ServeHTTP(ww, r)

			code := http.StatusText(ww.Status())
			metrics.RequestsTotal.WithLabelValues(r.URL.Path, code).Inc()
			metrics.RequestDuration.WithLabelValues(r.URL.Path, code).Observe(time.Since(start).Seconds())
		})
	}
}
