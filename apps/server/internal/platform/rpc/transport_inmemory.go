// Package rpc provides two http.RoundTripper implementations a cross-module
// ConnectRPC client can be built on: an in-memory one (calls stay inside this
// process, over a net.Pipe — no socket) and a network one (a real TCP call to
// another service, guarded by internal/platform/resilience).
//
// Today apps/server runs as a single monolith with no module calling another
// synchronously, so nothing wires either transport into a live request path
// yet — that's deliberate (see the module comparison this was ported from:
// building a synchronous cross-module call without a real one to carry would
// be inventing coupling that doesn't exist). What this package buys is that
// *when* a module needs to call another synchronously, the module code only
// ever depends on the target's generated Connect client interface; whether
// that interface is backed by NewInMemoryTransport (same process) or
// NewNetworkTransport (a separately deployed service) is decided once, in
// cmd/server's wiring, and never inside the module itself. That's what makes
// extracting a module into its own service later a cmd/-only change instead
// of a module rewrite.
package rpc

import (
	"context"
	"fmt"
	"net"
	"net/http"
	"sync"
)

// pipeListener is a net.Listener backed by net.Pipe instead of a real
// socket, so an in-process http.Server can serve a client that never
// touches the network stack, while still going through real HTTP/2 framing
// and serialization — the same code path a network call takes.
type pipeListener struct {
	conns  chan net.Conn
	once   sync.Once
	closed chan struct{}
}

func newPipeListener() *pipeListener {
	return &pipeListener{
		conns:  make(chan net.Conn),
		closed: make(chan struct{}),
	}
}

func (l *pipeListener) Accept() (net.Conn, error) {
	select {
	case conn := <-l.conns:
		return conn, nil
	case <-l.closed:
		return nil, net.ErrClosed
	}
}

func (l *pipeListener) Close() error {
	l.once.Do(func() { close(l.closed) })
	return nil
}

func (l *pipeListener) Addr() net.Addr { return pipeAddr{} }

// dial creates a fresh net.Pipe, hands the server side to a pending (or
// future) Accept call, and returns the client side.
func (l *pipeListener) dial(ctx context.Context) (net.Conn, error) {
	clientConn, serverConn := net.Pipe()
	select {
	case l.conns <- serverConn:
		return clientConn, nil
	case <-l.closed:
		_ = clientConn.Close()
		_ = serverConn.Close()
		return nil, net.ErrClosed
	case <-ctx.Done():
		_ = clientConn.Close()
		_ = serverConn.Close()
		return nil, ctx.Err()
	}
}

type pipeAddr struct{}

func (pipeAddr) Network() string { return "pipe" }
func (pipeAddr) String() string  { return "in-memory" }

// InMemoryTransport serves handler over an http.Server with no real socket,
// so a Connect client built on its RoundTripper reaches handler without a
// network hop — used by cmd/server to call one module's ConnectRPC handler
// from another's client, in-process.
type InMemoryTransport struct {
	listener *pipeListener
	server   *http.Server
}

// NewInMemoryTransport starts serving handler over an in-memory listener.
// The caller is responsible for calling Close when done (typically on
// shutdown, mirroring how cmd/server shuts down its real HTTP servers).
func NewInMemoryTransport(handler http.Handler) *InMemoryTransport {
	listener := newPipeListener()
	protocols := new(http.Protocols)
	protocols.SetUnencryptedHTTP2(true)

	server := &http.Server{
		Handler:   handler,
		Protocols: protocols,
	}

	t := &InMemoryTransport{listener: listener, server: server}
	go func() {
		_ = server.Serve(listener)
	}()
	return t
}

// RoundTripper returns an http.RoundTripper that dials directly into this
// transport's in-memory listener. Pass it as an http.Client's Transport when
// constructing a generated Connect client.
func (t *InMemoryTransport) RoundTripper() http.RoundTripper {
	return &http.Transport{
		DialContext: func(ctx context.Context, _, _ string) (net.Conn, error) {
			return t.listener.dial(ctx)
		},
		Protocols: t.server.Protocols,
	}
}

// Close shuts down the underlying http.Server and closes the listener.
func (t *InMemoryTransport) Close(ctx context.Context) error {
	if err := t.server.Shutdown(ctx); err != nil {
		return fmt.Errorf("rpc: shutdown in-memory transport: %w", err)
	}
	return nil
}
