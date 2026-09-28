package rpc

import (
	"net/http"

	"github.com/aedst/server/internal/platform/resilience"
)

// NewNetworkTransport returns an http.RoundTripper for a real network call
// to targetService, wrapped with a circuit breaker, rate limit and timeout
// (see internal/platform/resilience) so a downed dependency degrades this
// service instead of hanging every caller on it. Allows both HTTP/1.1 and
// unencrypted HTTP/2 (h2c), matching platform/httpserver.New's own
// Protocols configuration — same-cluster traffic between services behind a
// shared network boundary, no per-call TLS handshake needed.
func NewNetworkTransport(targetService string, cfg resilience.Config) http.RoundTripper {
	protocols := new(http.Protocols)
	protocols.SetHTTP1(true)
	protocols.SetUnencryptedHTTP2(true)

	base := &http.Transport{Protocols: protocols}

	if cfg.BreakerName == "" {
		cfg.BreakerName = targetService
	}
	return resilience.WrapRoundTripper(base, cfg)
}
