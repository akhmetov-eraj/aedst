package push

import (
	"bytes"
	"context"
	"crypto/aes"
	"crypto/cipher"
	"crypto/ecdh"
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/hkdf"
	"crypto/rand"
	"crypto/sha256"
	"encoding/base64"
	"encoding/binary"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"strconv"
	"strings"
	"time"

	"github.com/lestrrat-go/jwx/v4/jwa"
	"github.com/lestrrat-go/jwx/v4/jwt"
)

// WebPushConfig holds the VAPID application server keys (AEDST_PUSH_VAPID_*).
// Generate a pair once per deployment, e.g. `npx web-push generate-vapid-keys`.
type WebPushConfig struct {
	// PublicKey is the base64url uncompressed P-256 point (65 bytes); clients
	// pass it to PushManager.subscribe as applicationServerKey.
	PublicKey string
	// PrivateKey is the base64url raw P-256 scalar (32 bytes).
	PrivateKey string
	// Subject is a mailto: or https: contact for the push service operator.
	Subject string
}

// WebPush sends RFC 8291 (aes128gcm) encrypted messages with VAPID auth.
type WebPush struct {
	key       *ecdsa.PrivateKey
	publicKey string
	subject   string
	client    *http.Client
	now       func() time.Time
}

// maxWebPushPayload is the plaintext budget: push services must accept 4096
// bytes of ciphertext; header (86) + tag (16) + padding delimiter (1) leave this.
const maxWebPushPayload = 3993

func NewWebPush(cfg WebPushConfig, client *http.Client) (*WebPush, error) {
	d, err := base64.RawURLEncoding.DecodeString(strings.TrimRight(cfg.PrivateKey, "="))
	if err != nil {
		return nil, fmt.Errorf("push: vapid private key: %w", err)
	}
	key, err := ecdsa.ParseRawPrivateKey(elliptic.P256(), d)
	if err != nil {
		return nil, fmt.Errorf("push: vapid private key: %w", err)
	}
	pub, err := key.PublicKey.Bytes()
	if err != nil {
		return nil, fmt.Errorf("push: vapid public key: %w", err)
	}
	derived := base64.RawURLEncoding.EncodeToString(pub)
	if cfg.PublicKey != "" && strings.TrimRight(cfg.PublicKey, "=") != derived {
		return nil, errors.New("push: AEDST_PUSH_VAPID_PUBLIC_KEY does not match the private key")
	}
	if !strings.HasPrefix(cfg.Subject, "mailto:") && !strings.HasPrefix(cfg.Subject, "https://") {
		return nil, errors.New("push: AEDST_PUSH_VAPID_SUBJECT must be a mailto: or https: URL")
	}
	return &WebPush{key: key, publicKey: derived, subject: cfg.Subject, client: client, now: time.Now}, nil
}

// PublicKey is the applicationServerKey clients subscribe with.
func (w *WebPush) PublicKey() string { return w.publicKey }

// webPushPayload is the JSON the service worker receives (event.data.json()).
type webPushPayload struct {
	Title          string            `json:"title"`
	Body           string            `json:"body"`
	Link           string            `json:"link"`
	Kind           string            `json:"kind"`
	NotificationID string            `json:"notification_id"`
	Tag            string            `json:"tag,omitempty"`
	Data           map[string]string `json:"data,omitempty"`
}

func (w *WebPush) Send(ctx context.Context, target Target, msg Message) error {
	endpoint, err := url.Parse(target.Endpoint)
	if err != nil || endpoint.Scheme != "https" || endpoint.Host == "" {
		return fmt.Errorf("%w: web push endpoint must be an https URL", ErrInvalidToken)
	}
	plaintext, err := json.Marshal(webPushPayload{Title: msg.Title, Body: msg.Body, Link: msg.Link, Kind: msg.Kind, NotificationID: msg.NotificationID, Tag: msg.CollapseID, Data: msg.Data})
	if err != nil {
		return fmt.Errorf("%w: encode payload: %w", ErrPermanent, err)
	}
	if len(plaintext) > maxWebPushPayload {
		return fmt.Errorf("%w: web push payload is %d bytes (max %d)", ErrPermanent, len(plaintext), maxWebPushPayload)
	}
	body, err := encryptAES128GCM(target.P256dh, target.Auth, plaintext)
	if err != nil {
		return fmt.Errorf("%w: %w", ErrInvalidToken, err) // malformed subscription keys never become valid
	}
	token, err := w.vapidToken(endpoint.Scheme + "://" + endpoint.Host)
	if err != nil {
		return err
	}

	req, err := http.NewRequestWithContext(ctx, http.MethodPost, target.Endpoint, bytes.NewReader(body))
	if err != nil {
		return fmt.Errorf("push: web push request: %w", err)
	}
	ttl := msg.TTL
	if ttl <= 0 {
		ttl = 24 * time.Hour
	}
	req.Header.Set("Content-Encoding", "aes128gcm")
	req.Header.Set("Content-Type", "application/octet-stream")
	req.Header.Set("TTL", strconv.Itoa(int(ttl.Seconds())))
	req.Header.Set("Urgency", "normal")
	if msg.CollapseID != "" {
		req.Header.Set("Topic", topicHeader(msg.CollapseID))
	}
	req.Header.Set("Authorization", "vapid t="+token+", k="+w.publicKey)

	resp, err := w.client.Do(req)
	if err != nil {
		return fmt.Errorf("push: web push: %w", err)
	}
	defer func() { _ = resp.Body.Close() }()
	detail, _ := io.ReadAll(io.LimitReader(resp.Body, 512))
	return classify("web push", resp.StatusCode, string(detail))
}

// topicHeader makes a Topic header value: at most 32 URL-safe base64 chars.
func topicHeader(id string) string {
	sum := sha256.Sum256([]byte(id))
	return base64.RawURLEncoding.EncodeToString(sum[:])[:32]
}

// vapidToken signs the RFC 8292 JWT for the push service origin aud.
func (w *WebPush) vapidToken(aud string) (string, error) {
	now := w.now()
	tok, err := jwt.NewBuilder().
		Audience([]string{aud}).
		Expiration(now.Add(12 * time.Hour)).
		Subject(w.subject).
		Build()
	if err != nil {
		return "", fmt.Errorf("push: vapid token: %w", err)
	}
	signed, err := jwt.Sign(tok, jwt.WithKey(jwa.ES256(), w.key))
	if err != nil {
		return "", fmt.Errorf("push: sign vapid token: %w", err)
	}
	return string(signed), nil
}

// encryptAES128GCM implements RFC 8291 §3 (message encryption for Web Push)
// with the aes128gcm content coding of RFC 8188, as a single record.
func encryptAES128GCM(p256dh, authSecret string, plaintext []byte) ([]byte, error) {
	uaPublicBytes, err := decodeBase64URL(p256dh)
	if err != nil {
		return nil, fmt.Errorf("p256dh: %w", err)
	}
	auth, err := decodeBase64URL(authSecret)
	if err != nil || len(auth) != 16 {
		return nil, errors.New("auth secret must be 16 bytes")
	}
	curve := ecdh.P256()
	uaPublic, err := curve.NewPublicKey(uaPublicBytes)
	if err != nil {
		return nil, fmt.Errorf("p256dh: %w", err)
	}
	asPrivate, err := curve.GenerateKey(rand.Reader)
	if err != nil {
		return nil, fmt.Errorf("ephemeral key: %w", err)
	}
	asPublicBytes := asPrivate.PublicKey().Bytes()
	shared, err := asPrivate.ECDH(uaPublic)
	if err != nil {
		return nil, fmt.Errorf("ecdh: %w", err)
	}

	salt := make([]byte, 16)
	if _, err := rand.Read(salt); err != nil {
		return nil, fmt.Errorf("salt: %w", err)
	}
	keyInfo := "WebPush: info\x00" + string(uaPublicBytes) + string(asPublicBytes)
	ikm, err := hkdf.Key(sha256.New, shared, auth, keyInfo, 32)
	if err != nil {
		return nil, fmt.Errorf("hkdf ikm: %w", err)
	}
	prk, err := hkdf.Extract(sha256.New, ikm, salt)
	if err != nil {
		return nil, fmt.Errorf("hkdf prk: %w", err)
	}
	cek, err := hkdf.Expand(sha256.New, prk, "Content-Encoding: aes128gcm\x00", 16)
	if err != nil {
		return nil, fmt.Errorf("hkdf cek: %w", err)
	}
	nonce, err := hkdf.Expand(sha256.New, prk, "Content-Encoding: nonce\x00", 12)
	if err != nil {
		return nil, fmt.Errorf("hkdf nonce: %w", err)
	}

	block, err := aes.NewCipher(cek)
	if err != nil {
		return nil, fmt.Errorf("aes: %w", err)
	}
	gcm, err := cipher.NewGCM(block)
	if err != nil {
		return nil, fmt.Errorf("gcm: %w", err)
	}
	record := append(append([]byte{}, plaintext...), 0x02) // last-record padding delimiter

	header := make([]byte, 0, 16+4+1+len(asPublicBytes))
	header = append(header, salt...)
	header = binary.BigEndian.AppendUint32(header, 4096)
	header = append(header, byte(len(asPublicBytes)))
	header = append(header, asPublicBytes...)
	return gcm.Seal(header, nonce, record, nil), nil
}

func decodeBase64URL(s string) ([]byte, error) {
	s = strings.TrimRight(strings.TrimSpace(s), "=")
	if b, err := base64.RawURLEncoding.DecodeString(s); err == nil {
		return b, nil
	}
	b, err := base64.RawStdEncoding.DecodeString(s)
	if err != nil {
		return nil, fmt.Errorf("not base64: %w", err)
	}
	return b, nil
}
