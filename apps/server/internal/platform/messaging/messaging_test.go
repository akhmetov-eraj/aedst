package messaging_test

import (
	"context"
	"errors"
	"testing"

	"github.com/aedst/server/internal/platform/messaging"
)

func TestBus_Publish_FansOutToSubscribersOfTheType(t *testing.T) {
	bus := messaging.NewBus()
	var a, b, other int
	bus.Subscribe("x.v1.Thing", "a", func(context.Context, messaging.Event) error { a++; return nil })
	bus.Subscribe("x.v1.Thing", "b", func(context.Context, messaging.Event) error { b++; return nil })
	bus.Subscribe("x.v1.Other", "c", func(context.Context, messaging.Event) error { other++; return nil })

	if err := bus.Publish(context.Background(), messaging.Event{Type: "x.v1.Thing"}); err != nil {
		t.Fatalf("Publish() error = %v", err)
	}
	if a != 1 || b != 1 || other != 0 {
		t.Fatalf("handler calls a=%d b=%d other=%d, want 1 1 0", a, b, other)
	}
}

func TestBus_Publish_FailingHandlerDoesNotStarveOthersAndIsReported(t *testing.T) {
	bus := messaging.NewBus()
	boom := errors.New("boom")
	ran := false
	bus.Subscribe("t", "bad", func(context.Context, messaging.Event) error { return boom })
	bus.Subscribe("t", "good", func(context.Context, messaging.Event) error { ran = true; return nil })

	err := bus.Publish(context.Background(), messaging.Event{Type: "t"})
	if !errors.Is(err, boom) {
		t.Fatalf("Publish() error = %v, want it to wrap boom", err)
	}
	if !ran {
		t.Fatal("healthy subscriber did not run after a failing one")
	}
}

func TestBus_Publish_NoSubscribersIsNoOp(t *testing.T) {
	if err := messaging.NewBus().Publish(context.Background(), messaging.Event{Type: "nobody"}); err != nil {
		t.Fatalf("Publish() error = %v, want nil", err)
	}
}
