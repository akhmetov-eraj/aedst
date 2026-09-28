package rpc_test

import (
	"context"
	"io"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/aedst/server/internal/platform/resilience"
	"github.com/aedst/server/internal/platform/rpc"
)

// echoHandler is a stand-in for a module's generated ConnectRPC handler: any
// real http.Handler works identically over either transport, which is the
// point being verified here.
var echoHandler = http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "text/plain")
	w.WriteHeader(http.StatusOK)
	_, _ = w.Write([]byte("pong"))
})

func TestInMemoryTransport_RoundTripsToHandler(t *testing.T) {
	transport := rpc.NewInMemoryTransport(echoHandler)
	defer func() { _ = transport.Close(context.Background()) }()

	client := &http.Client{Transport: transport.RoundTripper(), Timeout: 5 * time.Second}
	resp, err := client.Get("http://in-memory/ping")
	if err != nil {
		t.Fatalf("GET: %v", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		t.Fatalf("read body: %v", err)
	}
	if string(body) != "pong" {
		t.Fatalf("expected %q, got %q", "pong", body)
	}
}

func TestNetworkTransport_RoundTripsToHandler(t *testing.T) {
	server := httptest.NewServer(echoHandler)
	defer server.Close()

	transport := rpc.NewNetworkTransport("echo", resilience.Config{
		Timeout:      5 * time.Second,
		MaxRequests:  1,
		RateLimitRPS: 100,
	})

	client := &http.Client{Transport: transport, Timeout: 5 * time.Second}
	resp, err := client.Get(server.URL + "/ping")
	if err != nil {
		t.Fatalf("GET: %v", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		t.Fatalf("read body: %v", err)
	}
	if string(body) != "pong" {
		t.Fatalf("expected %q, got %q", "pong", body)
	}
}

// TestBothTransports_SameHandler proves the point of this package: the same
// handler, reached through either transport, behaves identically — a caller
// built on the generated Connect client interface can't tell them apart.
func TestBothTransports_SameHandler(t *testing.T) {
	inMem := rpc.NewInMemoryTransport(echoHandler)
	defer func() { _ = inMem.Close(context.Background()) }()

	server := httptest.NewServer(echoHandler)
	defer server.Close()
	network := rpc.NewNetworkTransport("echo", resilience.Config{Timeout: 5 * time.Second, MaxRequests: 1, RateLimitRPS: 100})

	cases := []struct {
		name      string
		transport http.RoundTripper
		url       string
	}{
		{"in-memory", inMem.RoundTripper(), "http://in-memory/ping"},
		{"network", network, server.URL + "/ping"},
	}

	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			client := &http.Client{Transport: tc.transport, Timeout: 5 * time.Second}
			resp, err := client.Get(tc.url)
			if err != nil {
				t.Fatalf("GET: %v", err)
			}
			defer resp.Body.Close()

			body, err := io.ReadAll(resp.Body)
			if err != nil {
				t.Fatalf("read body: %v", err)
			}
			if string(body) != "pong" || resp.StatusCode != http.StatusOK {
				t.Fatalf("expected 200 %q, got %d %q", "pong", resp.StatusCode, body)
			}
		})
	}
}
