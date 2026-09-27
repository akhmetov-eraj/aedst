package observability

import (
	"context"
	"fmt"

	"go.opentelemetry.io/otel"
	"go.opentelemetry.io/otel/exporters/otlp/otlptrace/otlptracegrpc"
	"go.opentelemetry.io/otel/propagation"
	"go.opentelemetry.io/otel/sdk/resource"
	sdktrace "go.opentelemetry.io/otel/sdk/trace"
	semconv "go.opentelemetry.io/otel/semconv/v1.26.0"
	"go.opentelemetry.io/otel/trace"
)

// NewTracerProvider configures OpenTelemetry tracing. If otlpEndpoint is
// empty (e.g. local development without a Tempo collector running), traces
// are still created and propagated but exported nowhere, so instrumentation
// code never has to branch on whether tracing is "enabled".
func NewTracerProvider(ctx context.Context, serviceName, otlpEndpoint string) (*sdktrace.TracerProvider, error) {
	res, err := resource.Merge(resource.Default(), resource.NewSchemaless(
		semconv.ServiceName(serviceName),
	))
	if err != nil {
		return nil, fmt.Errorf("observability: build resource: %w", err)
	}

	opts := []sdktrace.TracerProviderOption{sdktrace.WithResource(res)}

	if otlpEndpoint != "" {
		exporter, err := otlptracegrpc.New(ctx,
			otlptracegrpc.WithEndpoint(otlpEndpoint),
			otlptracegrpc.WithInsecure(),
		)
		if err != nil {
			return nil, fmt.Errorf("observability: build otlp exporter: %w", err)
		}
		opts = append(opts, sdktrace.WithBatcher(exporter))
	}

	tp := sdktrace.NewTracerProvider(opts...)
	otel.SetTracerProvider(tp)
	otel.SetTextMapPropagator(propagation.NewCompositeTextMapPropagator(
		propagation.TraceContext{},
		propagation.Baggage{},
	))
	return tp, nil
}

// TraceIDFromContext returns the current span's trace ID, or "" if none.
func TraceIDFromContext(ctx context.Context) string {
	sc := trace.SpanContextFromContext(ctx)
	if !sc.HasTraceID() {
		return ""
	}
	return sc.TraceID().String()
}
