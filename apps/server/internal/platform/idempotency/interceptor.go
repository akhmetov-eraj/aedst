package idempotency

import (
	"context"
	"errors"

	"connectrpc.com/connect"
)

// HeaderName is the HTTP/Connect header a client sends its idempotency key
// on, matching the convention documented on every command RPC that accepts
// one (see docs/events.md).
const HeaderName = "Idempotency-Key"

// MaxKeyLength bounds a key: it is stored and indexed, so an unbounded header
// is a storage and lock-name abuse vector.
const MaxKeyLength = 128

var errInvalidKey = errors.New("Idempotency-Key must be 1-128 printable ASCII characters")

// HeaderInterceptor copies the Idempotency-Key request header onto the
// context via WithKey. It never rejects a request for lacking one — most RPCs
// don't need idempotency, so whether a key is required is left to each
// handler — but a malformed key (too long, control or non-ASCII characters)
// is rejected up front rather than stored.
func HeaderInterceptor() connect.UnaryInterceptorFunc {
	return func(next connect.UnaryFunc) connect.UnaryFunc {
		return func(ctx context.Context, req connect.AnyRequest) (connect.AnyResponse, error) {
			key := req.Header().Get(HeaderName)
			if key != "" && !validKey(key) {
				return nil, connect.NewError(connect.CodeInvalidArgument, errInvalidKey)
			}
			return next(WithKey(ctx, key), req)
		}
	}
}

func validKey(key string) bool {
	if len(key) > MaxKeyLength {
		return false
	}
	for i := 0; i < len(key); i++ {
		if key[i] < 0x21 || key[i] > 0x7e {
			return false
		}
	}
	return true
}
