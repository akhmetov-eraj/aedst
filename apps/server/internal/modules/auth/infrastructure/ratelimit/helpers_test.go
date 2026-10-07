package ratelimit_test

import (
	"context"
	"net/http"
	"net/http/httptest"
	"strconv"
	"strings"
)

type handlerFunc func(ctx context.Context)

func (f handlerFunc) ServeHTTP(_ http.ResponseWriter, r *http.Request) { f(r.Context()) }

func newRequest(remoteAddr string) (*http.Request, error) {
	req := httptest.NewRequest(http.MethodPost, "/", nil)
	req.RemoteAddr = remoteAddr
	return req, nil
}

func itoa(i int) string { return strconv.Itoa(i) }

func contains(s, sub string) bool { return strings.Contains(s, sub) }
