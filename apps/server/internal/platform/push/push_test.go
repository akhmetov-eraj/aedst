package push

import (
	"context"
	"crypto/aes"
	"crypto/cipher"
	"crypto/ecdh"
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/hkdf"
	"crypto/rand"
	"crypto/rsa"
	"crypto/sha256"
	"crypto/x509"
	"encoding/base64"
	"encoding/binary"
	"encoding/json"
	"encoding/pem"
	"errors"
	"io"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"github.com/lestrrat-go/jwx/v4/jwa"
	"github.com/lestrrat-go/jwx/v4/jwt"
)

func b64(b []byte) string { return base64.RawURLEncoding.EncodeToString(b) }

// decryptAES128GCM is the user-agent side of RFC 8291, used to prove the
// server side interoperates.
func decryptAES128GCM(t *testing.T, ua *ecdh.PrivateKey, auth, body []byte) []byte {
	t.Helper()
	salt, rs, idlen := body[:16], binary.BigEndian.Uint32(body[16:20]), int(body[20])
	if rs != 4096 || idlen != 65 {
		t.Fatalf("header rs=%d idlen=%d", rs, idlen)
	}
	asPublicBytes := body[21 : 21+idlen]
	asPublic, err := ecdh.P256().NewPublicKey(asPublicBytes)
	if err != nil {
		t.Fatal(err)
	}
	shared, err := ua.ECDH(asPublic)
	if err != nil {
		t.Fatal(err)
	}
	ikm, _ := hkdf.Key(sha256.New, shared, auth, "WebPush: info\x00"+string(ua.PublicKey().Bytes())+string(asPublicBytes), 32)
	prk, _ := hkdf.Extract(sha256.New, ikm, salt)
	cek, _ := hkdf.Expand(sha256.New, prk, "Content-Encoding: aes128gcm\x00", 16)
	nonce, _ := hkdf.Expand(sha256.New, prk, "Content-Encoding: nonce\x00", 12)
	block, _ := aes.NewCipher(cek)
	gcm, _ := cipher.NewGCM(block)
	plain, err := gcm.Open(nil, nonce, body[21+idlen:], nil)
	if err != nil {
		t.Fatalf("decrypt: %v", err)
	}
	if plain[len(plain)-1] != 0x02 {
		t.Fatalf("missing last-record delimiter")
	}
	return plain[:len(plain)-1]
}

func newVAPID(t *testing.T) (WebPushConfig, *ecdsa.PrivateKey) {
	t.Helper()
	key, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	if err != nil {
		t.Fatal(err)
	}
	d, _ := key.Bytes()
	pub, _ := key.PublicKey.Bytes()
	return WebPushConfig{PublicKey: b64(pub), PrivateKey: b64(d), Subject: "mailto:ops@example.com"}, key
}

func TestWebPush_EncryptsPerRFC8291AndSignsVAPID(t *testing.T) {
	cfg, vapidKey := newVAPID(t)
	ua, _ := ecdh.P256().GenerateKey(rand.Reader)
	auth := make([]byte, 16)
	_, _ = rand.Read(auth)

	var gotBody []byte
	var gotAuth, gotEncoding string
	srv := httptest.NewTLSServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		gotBody, _ = io.ReadAll(r.Body)
		gotAuth, gotEncoding = r.Header.Get("Authorization"), r.Header.Get("Content-Encoding")
		w.WriteHeader(http.StatusCreated)
	}))
	defer srv.Close()

	wp, err := NewWebPush(cfg, srv.Client())
	if err != nil {
		t.Fatal(err)
	}
	target := Target{Platform: PlatformWebPush, Endpoint: srv.URL + "/push/abc", P256dh: b64(ua.PublicKey().Bytes()), Auth: b64(auth)}
	if err := wp.Send(context.Background(), target, Message{Title: "Hi", Body: "there", Link: "/tasks/1", Kind: "task_completed", NotificationID: "n1"}); err != nil {
		t.Fatalf("Send() = %v", err)
	}
	if gotEncoding != "aes128gcm" {
		t.Errorf("Content-Encoding = %q", gotEncoding)
	}
	var payload webPushPayload
	if err := json.Unmarshal(decryptAES128GCM(t, ua, auth, gotBody), &payload); err != nil || payload.Title != "Hi" || payload.Link != "/tasks/1" || payload.Kind != "task_completed" || payload.NotificationID != "n1" {
		t.Fatalf("payload = %+v, %v", payload, err)
	}

	tPart, kPart, ok := strings.Cut(strings.TrimPrefix(gotAuth, "vapid t="), ", k=")
	if !ok || kPart != cfg.PublicKey {
		t.Fatalf("Authorization = %q", gotAuth)
	}
	tok, err := jwt.Parse([]byte(tPart), jwt.WithKey(jwa.ES256(), &vapidKey.PublicKey))
	if err != nil {
		t.Fatalf("vapid jwt does not verify: %v", err)
	}
	if aud, _ := tok.Audience(); len(aud) != 1 || aud[0] != srv.URL {
		t.Errorf("aud = %v, want the push service origin %s", aud, srv.URL)
	}
}

func TestWebPush_GoneDeactivates(t *testing.T) {
	cfg, _ := newVAPID(t)
	srv := httptest.NewTLSServer(http.HandlerFunc(func(w http.ResponseWriter, _ *http.Request) { w.WriteHeader(http.StatusGone) }))
	defer srv.Close()
	wp, _ := NewWebPush(cfg, srv.Client())
	ua, _ := ecdh.P256().GenerateKey(rand.Reader)
	err := wp.Send(context.Background(), Target{Endpoint: srv.URL, P256dh: b64(ua.PublicKey().Bytes()), Auth: b64(make([]byte, 16))}, Message{Title: "x"})
	if !errors.Is(err, ErrInvalidToken) {
		t.Fatalf("err = %v, want ErrInvalidToken", err)
	}
	if err := wp.Send(context.Background(), Target{Endpoint: "http://insecure.example"}, Message{}); !errors.Is(err, ErrInvalidToken) {
		t.Errorf("plain-http endpoint: err = %v", err)
	}
}

func TestNewWebPush_RejectsMismatchedKeys(t *testing.T) {
	cfg, _ := newVAPID(t)
	other, _ := newVAPID(t)
	cfg.PublicKey = other.PublicKey
	if _, err := NewWebPush(cfg, http.DefaultClient); err == nil {
		t.Fatal("want an error for a public key that does not match the private key")
	}
}

func TestFCM_SendsWithOAuthAndDetectsUnregistered(t *testing.T) {
	rsaKey, _ := rsa.GenerateKey(rand.Reader, 2048)
	der, _ := x509.MarshalPKCS8PrivateKey(rsaKey)
	keyPEM := string(pem.EncodeToMemory(&pem.Block{Type: "PRIVATE KEY", Bytes: der}))

	unregister := false
	mux := http.NewServeMux()
	mux.HandleFunc("/token", func(w http.ResponseWriter, r *http.Request) {
		_ = r.ParseForm()
		if r.Form.Get("grant_type") != "urn:ietf:params:oauth:grant-type:jwt-bearer" {
			http.Error(w, "bad grant", http.StatusBadRequest)
			return
		}
		w.Header().Set("Content-Type", "application/json")
		_, _ = w.Write([]byte(`{"access_token":"tok-123","token_type":"Bearer","expires_in":3600}`))
	})
	var got fcmRequest
	mux.HandleFunc("/v1/projects/demo/messages:send", func(w http.ResponseWriter, r *http.Request) {
		if r.Header.Get("Authorization") != "Bearer tok-123" {
			http.Error(w, "no auth", http.StatusUnauthorized)
			return
		}
		_ = json.NewDecoder(r.Body).Decode(&got)
		if unregister {
			w.WriteHeader(http.StatusNotFound)
			_, _ = w.Write([]byte(`{"error":{"code":404,"status":"NOT_FOUND","details":[{"errorCode":"UNREGISTERED"}]}}`))
			return
		}
		_, _ = w.Write([]byte(`{"name":"projects/demo/messages/1"}`))
	})
	srv := httptest.NewServer(mux)
	defer srv.Close()

	creds, _ := json.Marshal(serviceAccount{Type: "service_account", ProjectID: "demo", PrivateKey: keyPEM, ClientEmail: "fcm@demo.iam.gserviceaccount.com", TokenURI: srv.URL + "/token"})
	f, err := NewFCM(context.Background(), FCMConfig{CredentialsJSON: string(creds), BaseURL: srv.URL}, srv.Client())
	if err != nil {
		t.Fatal(err)
	}
	if err := f.Send(context.Background(), Target{Platform: PlatformFCM, Token: "device"}, Message{Title: "T", Body: "B", Link: "/x", Kind: "system", NotificationID: "n2", TTL: time.Hour}); err != nil {
		t.Fatalf("Send() = %v", err)
	}
	if got.Message.Token != "device" || got.Message.Data["link"] != "/x" || got.Message.Data["kind"] != "system" || got.Message.Data["title"] != "T" || got.Message.Data["body"] != "B" || got.Message.Data["notification_id"] != "n2" || got.Message.Android.TTL != "3600s" {
		t.Errorf("message = %+v", got.Message)
	}
	unregister = true
	if err := f.Send(context.Background(), Target{Platform: PlatformFCM, Token: "device"}, Message{Title: "T"}); !errors.Is(err, ErrInvalidToken) {
		t.Errorf("err = %v, want ErrInvalidToken", err)
	}
}

func TestAPNs_ProviderTokenAndInvalidDevice(t *testing.T) {
	key, _ := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	der, _ := x509.MarshalPKCS8PrivateKey(key)
	keyPEM := string(pem.EncodeToMemory(&pem.Block{Type: "PRIVATE KEY", Bytes: der}))

	var proto, topic, bearer string
	var apnsBody map[string]any
	srv := httptest.NewUnstartedServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		proto, topic, bearer = r.Proto, r.Header.Get("apns-topic"), strings.TrimPrefix(r.Header.Get("authorization"), "bearer ")
		_ = json.NewDecoder(r.Body).Decode(&apnsBody)
		if strings.HasSuffix(r.URL.Path, "/dead") {
			w.WriteHeader(http.StatusBadRequest)
			_, _ = w.Write([]byte(`{"reason":"BadDeviceToken"}`))
			return
		}
		if strings.HasSuffix(r.URL.Path, "/0000") {
			w.WriteHeader(http.StatusGone)
			_, _ = w.Write([]byte(`{"reason":"Unregistered"}`))
			return
		}
		w.WriteHeader(http.StatusOK)
	}))
	srv.EnableHTTP2 = true
	srv.StartTLS()
	defer srv.Close()

	a, err := NewAPNs(APNsConfig{KeyID: "KEY123", TeamID: "TEAM45", PrivateKey: keyPEM, DefaultTopic: "com.example.app", ProductionURL: srv.URL, SandboxURL: srv.URL}, srv.Client())
	if err != nil {
		t.Fatal(err)
	}
	if err := a.Send(context.Background(), Target{Platform: PlatformAPNs, Token: "abcd"}, Message{Title: "T", Link: "/tasks/1", Kind: "task_due_soon", NotificationID: "n3", Badge: new(3)}); err != nil {
		t.Fatalf("Send() = %v", err)
	}
	aps, _ := apnsBody["aps"].(map[string]any)
	if apnsBody["link"] != "/tasks/1" || apnsBody["kind"] != "task_due_soon" || apnsBody["notification_id"] != "n3" || aps["badge"] != float64(3) || aps["sound"] != "default" {
		t.Errorf("apns payload = %v", apnsBody)
	}
	if proto != "HTTP/2.0" || topic != "com.example.app" {
		t.Errorf("proto=%s topic=%s", proto, topic)
	}
	tok, err := jwt.Parse([]byte(bearer), jwt.WithKey(jwa.ES256(), &key.PublicKey))
	if err != nil {
		t.Fatalf("provider token does not verify: %v", err)
	}
	if iss, _ := tok.Issuer(); iss != "TEAM45" {
		t.Errorf("iss = %q", iss)
	}
	if err := a.Send(context.Background(), Target{Token: "0000"}, Message{Title: "T"}); !errors.Is(err, ErrInvalidToken) {
		t.Errorf("410: err = %v, want ErrInvalidToken", err)
	}
	if err := a.Send(context.Background(), Target{Token: "not-hex"}, Message{}); !errors.Is(err, ErrInvalidToken) {
		t.Errorf("non-hex token: err = %v", err)
	}
}

func TestDispatcher_DisabledProvider(t *testing.T) {
	d := &Dispatcher{}
	if err := d.Send(context.Background(), Target{Platform: PlatformFCM}, Message{}); !errors.Is(err, ErrDisabled) {
		t.Fatalf("err = %v, want ErrDisabled", err)
	}
	if len(d.Enabled()) != 0 {
		t.Error("nothing should be enabled")
	}
}
