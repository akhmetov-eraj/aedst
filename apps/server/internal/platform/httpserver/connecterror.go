package httpserver

import (
	"context"
	"errors"
	"log/slog"

	"connectrpc.com/connect"
	"go.opentelemetry.io/otel/codes"
	"go.opentelemetry.io/otel/trace"

	sharederrors "github.com/aedst/server/internal/shared/errors"
)

// internalError is the error inside a CodeInternal *connect.Error. Its message
// — the only thing a client ever sees — is fixed and generic, but it keeps the
// real cause reachable through Unwrap, so ErrorLogInterceptor can log and
// trace it server-side without it ever crossing the wire.
type internalError struct{ cause error }

func (internalError) Error() string   { return "internal error" }
func (e internalError) Unwrap() error { return e.cause }

// ToConnectError maps a module's domain error kinds onto ConnectRPC status
// codes without leaking internal error details (see project brief section
// 26 "Error Architecture"). Every module's transport layer uses this instead
// of hand-rolling its own mapping.
//
// Clients receive only the domain message (never the operation name or a
// wrapped infrastructure error). Anything unclassified becomes CodeInternal
// with a generic message and the cause attached for ErrorLogInterceptor.
func ToConnectError(err error) error {
	var connErr *connect.Error
	if errors.As(err, &connErr) {
		return connErr
	}

	switch {
	case errors.Is(err, context.Canceled):
		return connect.NewError(connect.CodeCanceled, errors.New("request canceled"))
	case errors.Is(err, context.DeadlineExceeded):
		return connect.NewError(connect.CodeDeadlineExceeded, errors.New("deadline exceeded"))
	}

	msg := clientMessage(err)
	switch sharederrors.KindOf(err) {
	case sharederrors.KindNotFound:
		return connect.NewError(connect.CodeNotFound, errors.New(msg))
	case sharederrors.KindAlreadyExists:
		return connect.NewError(connect.CodeAlreadyExists, errors.New(msg))
	case sharederrors.KindConflict:
		return connect.NewError(connect.CodeAborted, errors.New(msg))
	case sharederrors.KindValidation:
		return connect.NewError(connect.CodeInvalidArgument, errors.New(msg))
	case sharederrors.KindUnauthorized:
		return connect.NewError(connect.CodeUnauthenticated, errors.New(msg))
	case sharederrors.KindRateLimited:
		return connect.NewError(connect.CodeResourceExhausted, errors.New(msg))
	case sharederrors.KindForbidden:
		return connect.NewError(connect.CodePermissionDenied, errors.New(msg))
	case sharederrors.KindFailedPrecondition:
		return connect.NewError(connect.CodeFailedPrecondition, errors.New(msg))
	default:
		return connect.NewError(connect.CodeInternal, internalError{cause: err})
	}
}

// clientMessage is the safe-to-show text of a classified domain error.
func clientMessage(err error) string {
	var domainErr *sharederrors.Error
	if errors.As(err, &domainErr) && domainErr.Message != "" {
		return domainErr.Message
	}
	return "request failed"
}

// ErrorLogInterceptor logs every server-side failure with its real cause and
// records it on the request span. Without it a 500 is invisible: the client
// gets "internal error" by design and nothing else in the stack sees the
// cause. Client errors (invalid argument, not found, ...) are the caller's
// doing and are not logged here; cancellations by the client are not faults.
func ErrorLogInterceptor(logger *slog.Logger) connect.UnaryInterceptorFunc {
	return func(next connect.UnaryFunc) connect.UnaryFunc {
		return func(ctx context.Context, req connect.AnyRequest) (connect.AnyResponse, error) {
			resp, err := next(ctx, req)
			if err == nil {
				return resp, nil
			}
			code := connect.CodeOf(err)
			var level slog.Level
			switch code {
			case connect.CodeInternal, connect.CodeUnknown, connect.CodeDataLoss:
				level = slog.LevelError
			case connect.CodeDeadlineExceeded, connect.CodeUnavailable:
				level = slog.LevelWarn
			default:
				return resp, err
			}

			cause := causeOf(err)
			logger.Log(ctx, level, "rpc failed",
				slog.String("procedure", req.Spec().Procedure),
				slog.String("code", code.String()),
				slog.Any("error", cause),
			)
			if span := trace.SpanFromContext(ctx); span.IsRecording() {
				span.RecordError(cause)
				span.SetStatus(codes.Error, code.String())
			}
			return resp, err
		}
	}
}

// causeOf digs the server-side cause out of an error produced by ToConnectError.
func causeOf(err error) error {
	var ie internalError
	if errors.As(err, &ie) {
		return ie.cause
	}
	return err
}
