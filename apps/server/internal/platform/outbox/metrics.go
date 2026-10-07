package outbox

import (
	"context"
	"sync/atomic"
	"time"

	"go.opentelemetry.io/otel"
	"go.opentelemetry.io/otel/attribute"
	"go.opentelemetry.io/otel/metric"
)

// metrics are the relay's OpenTelemetry instruments, exported through the
// Prometheus reader observability.Setup wires (scrape /metrics) and OTLP.
//
//	aedst_outbox_pending{module}              rows owed delivery (capped, see BacklogCap)
//	aedst_outbox_oldest_pending_age_seconds   delivery lag; alert when it keeps growing
//	aedst_outbox_delivered_total{module}      events handed to subscribers
//	aedst_outbox_failed_total{module}         delivery attempts that errored (retried)
//	aedst_outbox_poisoned_total{module}       events given up on — page on any increase
//	aedst_outbox_batch_size{module}           rows claimed per relay pass
type metrics struct {
	delivered metric.Int64Counter
	failed    metric.Int64Counter
	poisoned  metric.Int64Counter
	batchSize metric.Int64Histogram
	attrs     metric.MeasurementOption

	pending atomic.Int64
	// oldestUnixNano is the commit time of the oldest pending row, 0 if none.
	oldestUnixNano atomic.Int64
}

func newMetrics(module string) (*metrics, error) {
	meter := otel.Meter("github.com/aedst/server/internal/platform/outbox")
	m := &metrics{attrs: metric.WithAttributes(attribute.String("module", module))}
	var err error
	if m.delivered, err = meter.Int64Counter("aedst_outbox_delivered_total", metric.WithDescription("Outbox events delivered to subscribers.")); err != nil {
		return nil, err
	}
	if m.failed, err = meter.Int64Counter("aedst_outbox_failed_total", metric.WithDescription("Outbox delivery attempts that failed and will be retried.")); err != nil {
		return nil, err
	}
	if m.poisoned, err = meter.Int64Counter("aedst_outbox_poisoned_total", metric.WithDescription("Outbox events abandoned after exhausting their attempts.")); err != nil {
		return nil, err
	}
	if m.batchSize, err = meter.Int64Histogram("aedst_outbox_batch_size", metric.WithDescription("Rows claimed per relay pass.")); err != nil {
		return nil, err
	}
	if _, err = meter.Int64ObservableGauge("aedst_outbox_pending",
		metric.WithDescription("Outbox rows awaiting delivery (capped)."),
		metric.WithInt64Callback(func(_ context.Context, o metric.Int64Observer) error {
			o.Observe(m.pending.Load(), m.attrs)
			return nil
		})); err != nil {
		return nil, err
	}
	if _, err = meter.Float64ObservableGauge("aedst_outbox_oldest_pending_age_seconds",
		metric.WithDescription("Age of the oldest undelivered outbox row; the delivery lag."),
		metric.WithUnit("s"),
		metric.WithFloat64Callback(func(_ context.Context, o metric.Float64Observer) error {
			age := 0.0
			if ns := m.oldestUnixNano.Load(); ns != 0 {
				age = time.Since(time.Unix(0, ns)).Seconds()
			}
			o.Observe(age, m.attrs)
			return nil
		})); err != nil {
		return nil, err
	}
	return m, nil
}

func (m *metrics) recordBacklog(b Backlog) {
	m.pending.Store(b.Pending)
	if b.Oldest.IsZero() {
		m.oldestUnixNano.Store(0)
		return
	}
	m.oldestUnixNano.Store(b.Oldest.UnixNano())
}
