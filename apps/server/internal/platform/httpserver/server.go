package httpserver

import (
	"context"
	"fmt"
	"net/http"
	"time"
)

// Server wraps an http.Server configured for unencrypted HTTP/2 (h2c), so
// ConnectRPC can serve gRPC (which requires HTTP/2) over plain-text locally,
// without TLS termination — matching production, where TLS terminates at the
// edge (Cloudflare/Envoy, see docs/deployment.md) and the app speaks h2c
// behind it. Uses net/http's native Protocols field (Go 1.24+) rather than
// the now-deprecated golang.org/x/net/http2/h2c package.
type Server struct {
	httpServer *http.Server
}

func New(addr string, handler http.Handler) *Server {
	protocols := new(http.Protocols)
	protocols.SetHTTP1(true)
	protocols.SetUnencryptedHTTP2(true)

	return &Server{
		httpServer: &http.Server{
			Addr:              addr,
			Handler:           handler,
			Protocols:         protocols,
			ReadHeaderTimeout: 10 * time.Second,
		},
	}
}

func (s *Server) ListenAndServe() error {
	if err := s.httpServer.ListenAndServe(); err != nil && err != http.ErrServerClosed {
		return fmt.Errorf("httpserver: listen: %w", err)
	}
	return nil
}

func (s *Server) Shutdown(ctx context.Context) error {
	return s.httpServer.Shutdown(ctx)
}
