package httpserver_test

import (
	"context"
	"errors"
	"fmt"
	"net"
	"syscall"
	"testing"

	"connectrpc.com/connect"

	"github.com/aedst/server/internal/platform/httpserver"
	sharederrors "github.com/aedst/server/internal/shared/errors"
)

func TestToConnectError_Classifications(t *testing.T) {
	tests := []struct {
		name string
		err  error
		code connect.Code
	}{
		{"not found", sharederrors.NotFound("op", "missing"), connect.CodeNotFound},
		{"dependency down", sharederrors.Wrap("op", &net.OpError{Op: "dial", Err: syscall.ECONNREFUSED}), connect.CodeUnavailable},
		{"dependency timeout", sharederrors.Wrap("op", fmt.Errorf("get: %w", context.DeadlineExceeded)), connect.CodeDeadlineExceeded},
		{"bug", sharederrors.Wrap("op", errors.New("boom")), connect.CodeInternal},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if got := connect.CodeOf(httpserver.ToConnectError(tt.err)); got != tt.code {
				t.Fatalf("code = %v, want %v", got, tt.code)
			}
		})
	}
}

func TestToConnectError_UnavailableTellsTheClientWhenToRetry(t *testing.T) {
	err := httpserver.ToConnectError(sharederrors.Wrap("op", &net.OpError{Op: "dial", Err: syscall.ECONNREFUSED}))
	var ce *connect.Error
	if !errors.As(err, &ce) {
		t.Fatalf("not a connect error: %v", err)
	}
	if got := ce.Meta().Get("Retry-After"); got == "" {
		t.Fatal("UNAVAILABLE must carry Retry-After")
	}
	if ce.Message() != "service temporarily unavailable" {
		t.Fatalf("message = %q: infrastructure detail must not reach the client", ce.Message())
	}
}
