package idempotency

import (
	"context"

	"connectrpc.com/connect"
)

// HeaderName is the HTTP/Connect header a client sends its idempotency key
// on, matching the convention documented on every command RPC that accepts
// one (see docs/events.md).
const HeaderName = "Idempotency-Key"

// HeaderInterceptor copies the Idempotency-Key request header onto the
// context via WithKey, for both unary and streaming calls. It never rejects
// a request for lacking one — most RPCs don't need idempotency, so whether a
// key is required is left to each handler.
func HeaderInterceptor() connect.UnaryInterceptorFunc {
	return func(next connect.UnaryFunc) connect.UnaryFunc {
		return func(ctx context.Context, req connect.AnyRequest) (connect.AnyResponse, error) {
			ctx = WithKey(ctx, req.Header().Get(HeaderName))
			return next(ctx, req)
		}
	}
}
