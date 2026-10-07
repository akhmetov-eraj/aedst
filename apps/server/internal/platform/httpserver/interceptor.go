package httpserver

import (
	"context"
	"errors"
	"log/slog"
	"net"
	"strings"
	"sync/atomic"
	"time"

	"connectrpc.com/connect"
	chimw "github.com/go-chi/chi/v5/middleware"

	platformauth "github.com/aedst/server/internal/platform/auth"
	"github.com/aedst/server/internal/platform/observability"
)

// AuthInterceptor verifies the bearer access token on every RPC except the
// procedures listed in publicProcedures (full name, e.g.
// "/aedst.auth.v1.AuthService/Login"), placing the resulting Identity on the
// context via platform/auth.WithIdentity. This is the only place a bearer
// token is verified — modules never verify tokens themselves. verifier may
// be the first-party TokenService, an OIDC resource-server verifier, or a
// MultiVerifier of both. Applied per-RPC (not per-route) so a service can mix
// public and authenticated procedures.
func AuthInterceptor(verifier platformauth.Verifier, publicProcedures map[string]bool, logger *slog.Logger) connect.UnaryInterceptorFunc {
	return func(next connect.UnaryFunc) connect.UnaryFunc {
		return func(ctx context.Context, req connect.AnyRequest) (connect.AnyResponse, error) {
			if publicProcedures[req.Spec().Procedure] {
				return next(ctx, req)
			}

			raw := bearerToken(req.Header().Get("Authorization"))
			if raw == "" {
				return nil, connect.NewError(connect.CodeUnauthenticated, errMissingToken)
			}

			claims, err := verifier.VerifyAccessToken(ctx, raw)
			if err != nil {
				// The reason stays server-side; clients only learn "invalid".
				logger.DebugContext(ctx, "auth: token rejected", slog.Any("error", err))
				return nil, connect.NewError(connect.CodeUnauthenticated, errInvalidToken)
			}

			ctx = platformauth.WithIdentity(ctx, platformauth.Identity{
				UserID:    claims.UserID,
				SessionID: claims.SessionID,
				DeviceID:  claims.DeviceID,
				Roles:     claims.Roles,
				Issuer:    claims.Issuer,
				Subject:   claims.Subject,
			})
			ctx = observability.WithUserID(ctx, claims.UserID.String())

			return next(ctx, req)
		}
	}
}

func bearerToken(header string) string {
	scheme, token, ok := strings.Cut(header, " ")
	if !ok || !strings.EqualFold(scheme, "Bearer") {
		return ""
	}
	return strings.TrimSpace(token)
}

var (
	errMissingToken = errors.New("missing bearer token")
	errInvalidToken = errors.New("invalid or expired token")
)

// RateLimiter is satisfied by valkey.RateLimiter.
type RateLimiter interface {
	Allow(ctx context.Context, key string, limit int, period time.Duration) (bool, error)
}

// RateLimitInterceptor throttles the listed procedures (typically the public
// authentication RPCs) per client address, cluster-wide via the shared
// limiter. It fails open on limiter errors (logged): rate limiting must not
// become a second way to take the API down.
func RateLimitInterceptor(limiter RateLimiter, procedures map[string]bool, perMinute int, logger *slog.Logger) connect.UnaryInterceptorFunc {
	warnFailOpen := failOpenWarner(logger)
	return func(next connect.UnaryFunc) connect.UnaryFunc {
		return func(ctx context.Context, req connect.AnyRequest) (connect.AnyResponse, error) {
			procedure := req.Spec().Procedure
			if !procedures[procedure] {
				return next(ctx, req)
			}

			client := chimw.GetClientIP(ctx) // set by the router; spoof-resistant
			if client == "" {
				client = clientHost(req.Peer().Addr)
			}
			key := "rl:" + procedure + ":" + client
			allowed, err := limiter.Allow(ctx, key, perMinute, time.Minute)
			if err != nil {
				warnFailOpen(ctx, err)
			} else if !allowed {
				return nil, connect.NewError(connect.CodeResourceExhausted, errors.New("rate limit exceeded, try again later"))
			}
			return next(ctx, req)
		}
	}
}

func clientHost(addr string) string {
	host, _, err := net.SplitHostPort(addr)
	if err != nil {
		return addr
	}
	return host
}

// UserRateLimitInterceptor throttles authenticated calls per user, cluster-wide
// through the shared limiter, so one runaway client or abusive account cannot
// exhaust capacity that belongs to everyone else. It must sit after
// AuthInterceptor; requests with no identity (the public procedures, which
// RateLimitInterceptor covers per client address) pass through. Like the
// per-address limiter it fails open: a limiter outage must not become an API
// outage.
func UserRateLimitInterceptor(limiter RateLimiter, perMinute int, logger *slog.Logger) connect.UnaryInterceptorFunc {
	warnFailOpen := failOpenWarner(logger)
	return func(next connect.UnaryFunc) connect.UnaryFunc {
		return func(ctx context.Context, req connect.AnyRequest) (connect.AnyResponse, error) {
			identity, ok := platformauth.FromContext(ctx)
			if !ok {
				return next(ctx, req)
			}
			allowed, err := limiter.Allow(ctx, "rl:user:"+identity.UserID.String(), perMinute, time.Minute)
			if err != nil {
				warnFailOpen(ctx, err)
			} else if !allowed {
				rateErr := connect.NewError(connect.CodeResourceExhausted, errors.New("rate limit exceeded, try again later"))
				rateErr.Meta().Set("Retry-After", "1")
				return nil, rateErr
			}
			return next(ctx, req)
		}
	}
}

// RoleInterceptor enforces role-based access: policy maps a procedure's full
// name (e.g. "/aedst.admin.v1.AdminService/ListUsers") to the roles of which
// the caller must hold at least one. Procedures absent from policy need only
// authentication. It must sit after AuthInterceptor, which is what puts the
// caller's roles on the context; a listed procedure reached without an
// identity is rejected as unauthenticated rather than waved through.
func RoleInterceptor(policy map[string][]string) connect.UnaryInterceptorFunc {
	return func(next connect.UnaryFunc) connect.UnaryFunc {
		return func(ctx context.Context, req connect.AnyRequest) (connect.AnyResponse, error) {
			required, restricted := requiredRoles(policy, req.Spec().Procedure)
			if !restricted {
				return next(ctx, req)
			}
			identity, ok := platformauth.FromContext(ctx)
			if !ok {
				return nil, connect.NewError(connect.CodeUnauthenticated, errMissingToken)
			}
			if !identity.HasAnyRole(required...) {
				return nil, connect.NewError(connect.CodePermissionDenied, errors.New("insufficient role"))
			}
			return next(ctx, req)
		}
	}
}

// failOpenWarnEvery bounds how often a limiter outage is logged.
const failOpenWarnEvery = 30 * time.Second

// failOpenWarner returns a logger for "the limiter is down, so requests are
// let through" that emits at most one line per failOpenWarnEvery. During a
// Valkey outage every request hits this path; one warning per request would
// bury the logs (and cost more than the outage itself) while adding nothing
// after the first.
func failOpenWarner(logger *slog.Logger) func(ctx context.Context, err error) {
	var last atomic.Int64 // unix nanos of the last emitted warning
	return func(ctx context.Context, err error) {
		now := time.Now().UnixNano()
		prev := last.Load()
		if now-prev < int64(failOpenWarnEvery) || !last.CompareAndSwap(prev, now) {
			return
		}
		logger.WarnContext(ctx, "rate limiter unavailable, failing open (logged at most once per 30s)", slog.Any("error", err))
	}
}
