package httpserver

import (
	"context"

	"connectrpc.com/connect"

	platformauth "github.com/aedst/server/internal/platform/auth"
	"github.com/aedst/server/internal/platform/observability"
)

// AuthInterceptor verifies the bearer access token on every RPC except the
// procedures listed in publicProcedures (full name, e.g.
// "/aedst.auth.v1.AuthService/Login"), placing the resulting Identity on the
// context via platform/auth.WithIdentity. This is the only place a bearer
// token is verified — modules never verify tokens themselves (see project
// brief section 17). Applied per-RPC (not per-route) so a service can mix
// public and authenticated procedures, e.g. UsersService.RegisterUser is
// public while UsersService.GetUser is not.
func AuthInterceptor(tokens *platformauth.TokenService, publicProcedures map[string]bool) connect.UnaryInterceptorFunc {
	return func(next connect.UnaryFunc) connect.UnaryFunc {
		return func(ctx context.Context, req connect.AnyRequest) (connect.AnyResponse, error) {
			if publicProcedures[req.Spec().Procedure] {
				return next(ctx, req)
			}

			raw := bearerToken(req.Header().Get("Authorization"))
			if raw == "" {
				return nil, connect.NewError(connect.CodeUnauthenticated, errMissingToken)
			}

			claims, err := tokens.VerifyAccessToken(ctx, raw)
			if err != nil {
				return nil, connect.NewError(connect.CodeUnauthenticated, errInvalidToken)
			}

			ctx = platformauth.WithIdentity(ctx, platformauth.Identity{
				UserID:    claims.UserID,
				SessionID: claims.SessionID,
				DeviceID:  claims.DeviceID,
				Roles:     claims.Roles,
			})
			ctx = observability.WithUserID(ctx, claims.UserID.String())

			return next(ctx, req)
		}
	}
}

func bearerToken(header string) string {
	const prefix = "Bearer "
	if len(header) > len(prefix) && header[:len(prefix)] == prefix {
		return header[len(prefix):]
	}
	return ""
}

var (
	errMissingToken = connectError("missing bearer token")
	errInvalidToken = connectError("invalid or expired token")
)

type connectError string

func (e connectError) Error() string { return string(e) }
