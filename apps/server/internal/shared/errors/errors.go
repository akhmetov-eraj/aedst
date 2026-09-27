// Package errors defines the small set of domain error kinds every module
// maps its own domain errors onto, so transport layers (ConnectRPC, HTTP) have
// one consistent place to translate errors into status codes without leaking
// internal details (see docs/architecture.md and the "Error Architecture"
// section of the project brief).
package errors

import (
	"errors"
	"fmt"
)

// Kind is a transport-agnostic classification of a domain error.
type Kind int

const (
	KindUnknown Kind = iota
	KindNotFound
	KindAlreadyExists
	KindConflict
	KindValidation
	KindUnauthorized
	KindForbidden
)

// Error is a domain error carrying a Kind so transport layers can map it to
// the right status code without string-matching messages.
type Error struct {
	Kind    Kind
	Op      string // the operation that failed, e.g. "profile.UpdateBio"
	Message string
	Err     error // wrapped cause, never exposed to clients directly
}

func (e *Error) Error() string {
	if e.Err != nil {
		return fmt.Sprintf("%s: %s: %v", e.Op, e.Message, e.Err)
	}
	return fmt.Sprintf("%s: %s", e.Op, e.Message)
}

func (e *Error) Unwrap() error { return e.Err }

func newErr(kind Kind, op, message string, cause error) *Error {
	return &Error{Kind: kind, Op: op, Message: message, Err: cause}
}

func NotFound(op, message string) *Error      { return newErr(KindNotFound, op, message, nil) }
func AlreadyExists(op, message string) *Error { return newErr(KindAlreadyExists, op, message, nil) }
func Conflict(op, message string) *Error      { return newErr(KindConflict, op, message, nil) }
func Validation(op, message string) *Error    { return newErr(KindValidation, op, message, nil) }
func Unauthorized(op, message string) *Error  { return newErr(KindUnauthorized, op, message, nil) }
func Forbidden(op, message string) *Error     { return newErr(KindForbidden, op, message, nil) }

// Wrap classifies an infrastructure error (e.g. a pgx error) as Unknown while
// preserving it as the cause, so it's still visible in logs but never exposed
// as a message string to clients.
func Wrap(op string, err error) *Error {
	return newErr(KindUnknown, op, "internal error", err)
}

// KindOf extracts the Kind from err, walking wrapped errors, defaulting to
// KindUnknown if err is not (or does not wrap) an *Error.
func KindOf(err error) Kind {
	var e *Error
	if errors.As(err, &e) {
		return e.Kind
	}
	return KindUnknown
}
