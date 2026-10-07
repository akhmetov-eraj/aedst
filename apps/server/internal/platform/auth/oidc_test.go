package auth_test

import (
	"context"
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rand"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/lestrrat-go/jwx/v4/jwa"
	"github.com/lestrrat-go/jwx/v4/jwk"
	"github.com/lestrrat-go/jwx/v4/jwt"

	"github.com/aedst/server/internal/platform/auth"
	"github.com/aedst/server/internal/shared/ids"
)

const testAudience = "aedst-api"

type fakeIdP struct {
	server *httptest.Server
	signer jwk.Key
}

func newFakeIdP(t *testing.T) *fakeIdP {
	t.Helper()

	priv, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	if err != nil {
		t.Fatal(err)
	}
	signer, err := jwk.Import[jwk.Key](priv)
	if err != nil {
		t.Fatal(err)
	}
	_ = signer.Set(jwk.KeyIDKey, "k1")
	pub, err := jwk.PublicKeyOf(signer)
	if err != nil {
		t.Fatal(err)
	}
	_ = pub.Set(jwk.AlgorithmKey, jwa.ES256())
	set := jwk.NewSet()
	_ = set.AddKey(pub)

	idp := &fakeIdP{signer: signer}
	mux := http.NewServeMux()
	idp.server = httptest.NewServer(mux)
	t.Cleanup(idp.server.Close)

	mux.HandleFunc("/.well-known/openid-configuration", func(w http.ResponseWriter, _ *http.Request) {
		_ = json.NewEncoder(w).Encode(map[string]string{"issuer": idp.server.URL, "jwks_uri": idp.server.URL + "/jwks"})
	})
	mux.HandleFunc("/jwks", func(w http.ResponseWriter, _ *http.Request) {
		_ = json.NewEncoder(w).Encode(set)
	})
	return idp
}

func (i *fakeIdP) token(t *testing.T, mutate func(*jwt.Builder)) string {
	t.Helper()
	now := time.Now()
	b := jwt.NewBuilder().
		Issuer(i.server.URL).
		Subject("user-123").
		Audience([]string{testAudience}).
		IssuedAt(now).
		Expiration(now.Add(time.Hour)).
		Claim("roles", []string{"admin"})
	if mutate != nil {
		mutate(b)
	}
	tok, err := b.Build()
	if err != nil {
		t.Fatal(err)
	}
	signed, err := jwt.Sign(tok, jwt.WithKey(jwa.ES256(), i.signer))
	if err != nil {
		t.Fatal(err)
	}
	return string(signed)
}

func newVerifier(t *testing.T, idp *fakeIdP) *auth.OIDCVerifier {
	t.Helper()
	v, err := auth.NewOIDCVerifier(auth.OIDCConfig{
		Issuer:     idp.server.URL,
		Audience:   testAudience,
		Algorithms: []string{"ES256"},
		RolesClaim: "roles",
	})
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { _ = v.Close(context.Background()) })
	return v
}

func TestOIDCVerifier_AcceptsValidToken(t *testing.T) {
	idp := newFakeIdP(t)
	v := newVerifier(t, idp)

	claims, err := v.VerifyAccessToken(context.Background(), idp.token(t, nil))
	if err != nil {
		t.Fatalf("VerifyAccessToken() error = %v", err)
	}
	if claims.Subject != "user-123" || claims.UserID != ids.FromExternal(idp.server.URL, "user-123") {
		t.Errorf("claims = %+v, want subject user-123 mapped via ids.FromExternal", claims)
	}
	if len(claims.Roles) != 1 || claims.Roles[0] != "admin" {
		t.Errorf("roles = %v, want [admin]", claims.Roles)
	}
}

func TestOIDCVerifier_RejectsWrongAudienceIssuerAndExpiry(t *testing.T) {
	idp := newFakeIdP(t)
	v := newVerifier(t, idp)

	cases := map[string]func(*jwt.Builder){
		"wrong audience": func(b *jwt.Builder) { b.Audience([]string{"someone-else"}) },
		"wrong issuer":   func(b *jwt.Builder) { b.Issuer("https://evil.example") },
		"expired":        func(b *jwt.Builder) { b.Expiration(time.Now().Add(-time.Hour)) },
	}
	for name, mutate := range cases {
		if _, err := v.VerifyAccessToken(context.Background(), idp.token(t, mutate)); err == nil {
			t.Errorf("%s: VerifyAccessToken() accepted an invalid token", name)
		}
	}
}

func TestOIDCVerifier_RejectsDisallowedAlgorithm(t *testing.T) {
	idp := newFakeIdP(t)
	v := newVerifier(t, idp)

	// An HS256 token "signed" with a public-key-shaped secret is the classic
	// algorithm-confusion attack; it must never reach signature verification.
	hmacKey, err := jwk.Import[jwk.Key]([]byte("0123456789abcdef0123456789abcdef"))
	if err != nil {
		t.Fatal(err)
	}
	tok, _ := jwt.NewBuilder().Issuer(idp.server.URL).Subject("x").Audience([]string{testAudience}).Expiration(time.Now().Add(time.Hour)).Build()
	signed, err := jwt.Sign(tok, jwt.WithKey(jwa.HS256(), hmacKey))
	if err != nil {
		t.Fatal(err)
	}
	if _, err := v.VerifyAccessToken(context.Background(), string(signed)); err == nil {
		t.Fatal("VerifyAccessToken() accepted an HS256 token for an asymmetric-only issuer")
	}
}

func TestOIDCVerifier_UnreachableProviderFailsClosed(t *testing.T) {
	idp := newFakeIdP(t)
	token := idp.token(t, nil)
	v := newVerifier(t, idp)
	idp.server.Close()

	if _, err := v.VerifyAccessToken(context.Background(), token); err == nil {
		t.Fatal("VerifyAccessToken() succeeded while the provider was unreachable")
	}
}

func TestMultiVerifier_RoutesByIssuer(t *testing.T) {
	idp := newFakeIdP(t)
	oidc := newVerifier(t, idp)
	first := newTestService(t, "test-signing-key-at-least-32-bytes!!", "")
	multi, err := auth.NewMultiVerifier(first, oidc)
	if err != nil {
		t.Fatal(err)
	}

	pair, err := first.IssuePair(ids.New(), ids.New(), "d", []string{"user"})
	if err != nil {
		t.Fatal(err)
	}
	if _, err := multi.VerifyAccessToken(context.Background(), pair.AccessToken); err != nil {
		t.Errorf("first-party token rejected: %v", err)
	}
	if _, err := multi.VerifyAccessToken(context.Background(), idp.token(t, nil)); err != nil {
		t.Errorf("oidc token rejected: %v", err)
	}
	if _, err := multi.VerifyAccessToken(context.Background(), idp.token(t, func(b *jwt.Builder) { b.Issuer("https://unknown.example") })); err == nil {
		t.Error("token from an unknown issuer was accepted")
	}
}

func TestIDsFromExternal_UUIDSubjectPassesThroughOthersAreStable(t *testing.T) {
	u := ids.New()
	if got := ids.FromExternal("iss", u.String()); got != u {
		t.Errorf("UUID subject mapped to %s, want %s", got, u)
	}
	a, b := ids.FromExternal("iss", "12345"), ids.FromExternal("iss", "12345")
	if a != b || a == ids.FromExternal("other-iss", "12345") {
		t.Error("non-UUID subject mapping must be deterministic and issuer-scoped")
	}
}

func TestOIDCVerifier_RolesClaimShapes(t *testing.T) {
	idp := newFakeIdP(t)

	tests := []struct {
		name      string
		claim     string
		set       func(*jwt.Builder)
		wantRoles []string
	}{
		{"keycloak dotted path", "realm_access.roles", func(b *jwt.Builder) {
			b.Claim("realm_access", map[string]any{"roles": []string{"user", "admin"}})
		}, []string{"user", "admin"}},
		{"zitadel object keyed by role", "urn:zitadel:iam:org:project:roles", func(b *jwt.Builder) {
			b.Claim("urn:zitadel:iam:org:project:roles", map[string]any{"editor": map[string]any{"org": "x"}, "admin": map[string]any{}})
		}, []string{"admin", "editor"}},
		{"claim name containing dots wins over path splitting", "https://example.com/roles", func(b *jwt.Builder) {
			b.Claim("https://example.com/roles", []string{"a"})
		}, []string{"a"}},
		{"space separated string", "scope", func(b *jwt.Builder) { b.Claim("scope", "read write") }, []string{"read", "write"}},
		{"missing claim", "nope.nothing", func(*jwt.Builder) {}, nil},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			v, err := auth.NewOIDCVerifier(auth.OIDCConfig{
				Issuer: idp.server.URL, Audience: testAudience, Algorithms: []string{"ES256"}, RolesClaim: tt.claim,
			})
			if err != nil {
				t.Fatal(err)
			}
			t.Cleanup(func() { _ = v.Close(context.Background()) })

			claims, err := v.VerifyAccessToken(context.Background(), idp.token(t, tt.set))
			if err != nil {
				t.Fatalf("VerifyAccessToken() error = %v", err)
			}
			if len(claims.Roles) != len(tt.wantRoles) {
				t.Fatalf("roles = %v, want %v", claims.Roles, tt.wantRoles)
			}
			for i := range tt.wantRoles {
				if claims.Roles[i] != tt.wantRoles[i] {
					t.Fatalf("roles = %v, want %v", claims.Roles, tt.wantRoles)
				}
			}
		})
	}
}
