package httpserver

import (
	"errors"

	"connectrpc.com/connect"

	sharederrors "github.com/aedst/server/internal/shared/errors"
)

// ToConnectError maps a module's domain error kinds onto ConnectRPC status
// codes without leaking internal error details (see project brief section
// 26 "Error Architecture"). Every module's transport layer uses this instead
// of hand-rolling its own mapping.
func ToConnectError(err error) error {
	var connErr *connect.Error
	if errors.As(err, &connErr) {
		return connErr
	}

	switch sharederrors.KindOf(err) {
	case sharederrors.KindNotFound:
		return connect.NewError(connect.CodeNotFound, err)
	case sharederrors.KindAlreadyExists:
		return connect.NewError(connect.CodeAlreadyExists, err)
	case sharederrors.KindConflict:
		return connect.NewError(connect.CodeAborted, err)
	case sharederrors.KindValidation:
		return connect.NewError(connect.CodeInvalidArgument, err)
	case sharederrors.KindUnauthorized:
		return connect.NewError(connect.CodeUnauthenticated, err)
	case sharederrors.KindForbidden:
		return connect.NewError(connect.CodePermissionDenied, err)
	default:
		return connect.NewError(connect.CodeInternal, errors.New("internal error"))
	}
}
