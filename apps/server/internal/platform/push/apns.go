package push

import (
	"bytes"
	"context"
	"crypto/ecdsa"
	"crypto/x509"
	"encoding/hex"
	"encoding/json"
	"encoding/pem"
	"errors"
	"fmt"
	"io"
	"net/http"
	"os"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/lestrrat-go/jwx/v4/jwa"
	"github.com/lestrrat-go/jwx/v4/jws"
	"github.com/lestrrat-go/jwx/v4/jwt"
)

const (
	apnsProductionURL = "https://api.push.apple.com"
	apnsSandboxURL    = "https://api.sandbox.push.apple.com"
	// Apple accepts a provider token for up to an hour and rejects refreshing
	// it more often than every 20 minutes.
	apnsTokenLifetime = 50 * time.Minute
)

// APNsConfig configures token-based APNs auth (AEDST_PUSH_APNS_*), from the
// .p8 key created in the Apple Developer account.
type APNsConfig struct {
	KeyID  string
	TeamID string
	// PrivateKey is the PEM (.p8) content; PrivateKeyFile a path to it.
	PrivateKey     string
	PrivateKeyFile string
	// DefaultTopic is the bundle id used when a device registered none.
	DefaultTopic string
	// ProductionURL / SandboxURL are for tests.
	ProductionURL string
	SandboxURL    string
}

// APNs sends over HTTP/2 with an ES256 provider token signed by jwx.
type APNs struct {
	cfg    APNsConfig
	key    *ecdsa.PrivateKey
	client *http.Client
	now    func() time.Time

	mu       sync.Mutex
	token    string
	issuedAt time.Time
}

func NewAPNs(cfg APNsConfig, client *http.Client) (*APNs, error) {
	raw := []byte(cfg.PrivateKey)
	if len(raw) == 0 && cfg.PrivateKeyFile != "" {
		var err error
		if raw, err = os.ReadFile(cfg.PrivateKeyFile); err != nil {
			return nil, fmt.Errorf("push: apns key: %w", err)
		}
	}
	block, _ := pem.Decode(raw)
	if block == nil {
		return nil, errors.New("push: apns key is not PEM (.p8)")
	}
	parsed, err := x509.ParsePKCS8PrivateKey(block.Bytes)
	if err != nil {
		return nil, fmt.Errorf("push: apns key: %w", err)
	}
	key, ok := parsed.(*ecdsa.PrivateKey)
	if !ok {
		return nil, errors.New("push: apns key must be an EC P-256 key")
	}
	if cfg.KeyID == "" || cfg.TeamID == "" {
		return nil, errors.New("push: AEDST_PUSH_APNS_KEY_ID and AEDST_PUSH_APNS_TEAM_ID are required")
	}
	if cfg.ProductionURL == "" {
		cfg.ProductionURL = apnsProductionURL
	}
	if cfg.SandboxURL == "" {
		cfg.SandboxURL = apnsSandboxURL
	}
	return &APNs{cfg: cfg, key: key, client: client, now: time.Now}, nil
}

// providerToken returns the cached token, re-signing it when it is older than
// apnsTokenLifetime (or when force is set after an ExpiredProviderToken).
func (a *APNs) providerToken(force bool) (string, error) {
	a.mu.Lock()
	defer a.mu.Unlock()
	now := a.now()
	if !force && a.token != "" && now.Sub(a.issuedAt) < apnsTokenLifetime {
		return a.token, nil
	}
	tok, err := jwt.NewBuilder().Issuer(a.cfg.TeamID).IssuedAt(now).Build()
	if err != nil {
		return "", fmt.Errorf("push: apns token: %w", err)
	}
	headers := jws.NewHeaders()
	if err := headers.Set(jws.KeyIDKey, a.cfg.KeyID); err != nil {
		return "", fmt.Errorf("push: apns token kid: %w", err)
	}
	signed, err := jwt.Sign(tok, jwt.WithKey(jwa.ES256(), a.key, jws.WithProtectedHeaders(headers)))
	if err != nil {
		return "", fmt.Errorf("push: sign apns token: %w", err)
	}
	a.token, a.issuedAt = string(signed), now
	return a.token, nil
}

type apnsPayload struct {
	APS            apnsAPS           `json:"aps"`
	Link           string            `json:"link"`
	Kind           string            `json:"kind"`
	NotificationID string            `json:"notification_id"`
	Data           map[string]string `json:"data,omitempty"`
}

type apnsAPS struct {
	Alert    apnsAlert `json:"alert"`
	Badge    *int      `json:"badge,omitempty"`
	Sound    string    `json:"sound,omitempty"`
	ThreadID string    `json:"thread-id,omitempty"`
}

type apnsAlert struct {
	Title string `json:"title,omitempty"`
	Body  string `json:"body,omitempty"`
}

func (a *APNs) Send(ctx context.Context, target Target, msg Message) error {
	if _, err := hex.DecodeString(target.Token); err != nil || target.Token == "" {
		return fmt.Errorf("%w: apns token must be hex", ErrInvalidToken)
	}
	topic := target.APNsTopic
	if topic == "" {
		topic = a.cfg.DefaultTopic
	}
	if topic == "" {
		return fmt.Errorf("%w: apns topic (bundle id) unknown; set AEDST_PUSH_APNS_TOPIC", ErrPermanent)
	}
	payload, err := json.Marshal(apnsPayload{
		APS:            apnsAPS{Alert: apnsAlert{Title: msg.Title, Body: msg.Body}, Badge: msg.Badge, Sound: "default"},
		Link:           msg.Link,
		Kind:           msg.Kind,
		NotificationID: msg.NotificationID,
		Data:           msg.Data,
	})
	if err != nil {
		return fmt.Errorf("%w: encode apns payload: %w", ErrPermanent, err)
	}
	base := a.cfg.ProductionURL
	if target.APNsSandbox {
		base = a.cfg.SandboxURL
	}

	send := func(force bool) (int, string, error) {
		token, err := a.providerToken(force)
		if err != nil {
			return 0, "", err
		}
		req, err := http.NewRequestWithContext(ctx, http.MethodPost, strings.TrimRight(base, "/")+"/3/device/"+target.Token, bytes.NewReader(payload))
		if err != nil {
			return 0, "", fmt.Errorf("push: apns request: %w", err)
		}
		req.Header.Set("authorization", "bearer "+token)
		req.Header.Set("apns-topic", topic)
		req.Header.Set("apns-push-type", "alert")
		req.Header.Set("apns-priority", "10")
		if msg.TTL > 0 {
			req.Header.Set("apns-expiration", strconv.FormatInt(a.now().Add(msg.TTL).Unix(), 10))
		}
		if msg.CollapseID != "" && len(msg.CollapseID) <= 64 {
			req.Header.Set("apns-collapse-id", msg.CollapseID)
		}
		resp, err := a.client.Do(req)
		if err != nil {
			return 0, "", fmt.Errorf("push: apns: %w", err)
		}
		defer func() { _ = resp.Body.Close() }()
		var body struct {
			Reason string `json:"reason"`
		}
		raw, _ := io.ReadAll(io.LimitReader(resp.Body, 1024))
		_ = json.Unmarshal(raw, &body)
		return resp.StatusCode, body.Reason, nil
	}

	status, reason, err := send(false)
	if err != nil {
		return err
	}
	if status == http.StatusForbidden && reason == "ExpiredProviderToken" {
		if status, reason, err = send(true); err != nil {
			return err
		}
	}
	switch reason {
	case "BadDeviceToken", "Unregistered", "DeviceTokenNotForTopic":
		return fmt.Errorf("%w: apns %s", ErrInvalidToken, reason)
	}
	return classify("apns", status, reason)
}
