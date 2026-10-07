// Package push delivers push notifications to the three platform channels a
// product needs — Web Push (RFC 8030/8291 + VAPID RFC 8292), Firebase Cloud
// Messaging HTTP v1 and Apple Push Notification service (HTTP/2, token auth).
// It is transport only: which notification goes to which device, retries and
// device bookkeeping belong to the notifications module, which calls Send
// from a River job.
//
// Every provider is optional and disabled when unconfigured; Send to a
// disabled provider returns ErrDisabled so the caller can skip the device.
//
// Why no third-party push libraries: the maintained Web Push library
// (SherClockHolmes/webpush-go) depends on golang-jwt, a second JOSE library
// that the stack forbids; FCM and APNs are each one authenticated HTTPS POST.
// VAPID and APNs provider tokens are signed with the project's single JOSE
// library (lestrrat-go/jwx v4); the RFC 8291 payload encryption uses only the
// standard library (crypto/ecdh, crypto/hkdf, AES-GCM).
package push

import (
	"context"
	"errors"
	"fmt"
	"net/http"
	"time"

	"go.opentelemetry.io/contrib/instrumentation/net/http/otelhttp"
)

// Platform is a push channel.
type Platform string

const (
	PlatformWebPush Platform = "web_push"
	PlatformFCM     Platform = "fcm"
	PlatformAPNs    Platform = "apns"
)

var (
	// ErrInvalidToken means the device registration is permanently gone
	// (unsubscribed, uninstalled, wrong environment): deactivate the device.
	ErrInvalidToken = errors.New("push: device token is invalid or expired")
	// ErrPermanent means retrying cannot help (bad payload, bad credentials);
	// the delivery should be dropped and the cause logged.
	ErrPermanent = errors.New("push: permanent delivery failure")
	// ErrDisabled means the provider for the device's platform is not
	// configured on this deployment.
	ErrDisabled = errors.New("push: provider not configured")
)

// Target is one device registration.
type Target struct {
	Platform Platform
	// Token is the FCM registration token or the APNs device token (hex).
	Token string
	// Web Push subscription.
	Endpoint string
	P256dh   string
	Auth     string
	// APNs only.
	APNsTopic   string
	APNsSandbox bool
}

// Message is what is shown to the user plus app data.
type Message struct {
	Title string
	Body  string
	// Link is the in-app route opened on tap ("/tasks/<id>").
	Link string
	// CollapseID lets a newer message replace an older one on the device.
	CollapseID string
	// Kind and NotificationID are part of the client payload contract (see
	// "Payload contract" below).
	Kind           string
	NotificationID string
	// Badge, when non-nil, sets the app icon badge (APNs aps.badge).
	Badge *int
	// Data is extra key/value data delivered to the app.
	Data map[string]string
	// TTL bounds how long the provider keeps an undeliverable message.
	TTL time.Duration
}

// Payload contract (what the web, Android and Apple clients parse):
//
//   - FCM: `data` = {kind, title, body, link, notification_id} (+ Data extras),
//     plus a `notification` block {title, body}.
//   - APNs: `aps` = {alert: {title, body}, badge: unread count, sound: default},
//     with top-level custom keys `link`, `kind`, `notification_id`.
//   - Web Push: encrypted JSON {title, body, link, kind, notification_id}.
//
// link is always a relative in-app route (/tasks/<id>).

// Sender delivers one message to one device.
type Sender interface {
	Send(ctx context.Context, target Target, msg Message) error
}

// Dispatcher routes a Target to the provider of its platform. A nil provider
// is disabled.
type Dispatcher struct {
	WebPush Sender
	FCM     Sender
	APNs    Sender
}

var _ Sender = (*Dispatcher)(nil)

func (d *Dispatcher) Send(ctx context.Context, target Target, msg Message) error {
	var s Sender
	switch target.Platform {
	case PlatformWebPush:
		s = d.WebPush
	case PlatformFCM:
		s = d.FCM
	case PlatformAPNs:
		s = d.APNs
	default:
		return fmt.Errorf("%w: unknown platform %q", ErrPermanent, target.Platform)
	}
	if s == nil {
		return ErrDisabled
	}
	return s.Send(ctx, target, msg)
}

// Enabled lists the configured platforms.
func (d *Dispatcher) Enabled() []Platform {
	var out []Platform
	if d.WebPush != nil {
		out = append(out, PlatformWebPush)
	}
	if d.FCM != nil {
		out = append(out, PlatformFCM)
	}
	if d.APNs != nil {
		out = append(out, PlatformAPNs)
	}
	return out
}

// NewHTTPClient is the client every provider uses: traced, HTTP/2 when the
// server offers it (APNs requires it), bounded by timeout.
func NewHTTPClient(timeout time.Duration) *http.Client {
	transport := http.DefaultTransport.(*http.Transport).Clone()
	transport.ForceAttemptHTTP2 = true
	return &http.Client{Timeout: timeout, Transport: otelhttp.NewTransport(transport)}
}

// classify maps a provider's HTTP status to the package's error kinds for
// the statuses every provider treats alike.
func classify(provider string, status int, detail string) error {
	switch {
	case status >= 200 && status < 300:
		return nil
	case status == http.StatusNotFound || status == http.StatusGone:
		return fmt.Errorf("%w: %s %d %s", ErrInvalidToken, provider, status, detail)
	case status == http.StatusTooManyRequests || status >= 500:
		return fmt.Errorf("push: %s transient %d %s", provider, status, detail)
	default:
		return fmt.Errorf("%w: %s %d %s", ErrPermanent, provider, status, detail)
	}
}
