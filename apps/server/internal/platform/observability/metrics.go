package observability

import (
	"net/http"

	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promauto"
	"github.com/prometheus/client_golang/prometheus/promhttp"
)

// Metrics holds the process-wide HTTP metrics. Modules that need custom
// metrics register their own with promauto and the default registry rather
// than going through this struct.
type Metrics struct {
	RequestsTotal   *prometheus.CounterVec
	RequestDuration *prometheus.HistogramVec
}

func NewMetrics() *Metrics {
	return &Metrics{
		RequestsTotal: promauto.NewCounterVec(prometheus.CounterOpts{
			Name: "aedst_http_requests_total",
			Help: "Total HTTP/RPC requests processed.",
		}, []string{"procedure", "code"}),
		RequestDuration: promauto.NewHistogramVec(prometheus.HistogramOpts{
			Name:    "aedst_http_request_duration_seconds",
			Help:    "Request duration in seconds.",
			Buckets: prometheus.DefBuckets,
		}, []string{"procedure", "code"}),
	}
}

// Handler returns the /metrics HTTP handler for Prometheus scraping.
func Handler() http.Handler {
	return promhttp.Handler()
}
