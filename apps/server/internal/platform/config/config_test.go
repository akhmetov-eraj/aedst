package config_test

import (
	"strings"
	"testing"

	"github.com/aedst/server/internal/platform/config"
)

func validConfig() config.Config {
	return config.Config{
		Env:       config.EnvDevelopment,
		HTTP:      config.HTTPConfig{RequestTimeout: 30, MaxRequestBytes: 1 << 20, AllowedOrigins: []string{"http://localhost:3000"}},
		DB:        config.DatabaseConfig{DSN: "postgres://x", MaxConns: 20, MinConns: 2, JobsMaxConns: 10},
		Outbox:    config.OutboxConfig{RelayIntervalMs: 500, BatchSize: 100, Concurrency: 4, RetentionHours: 168},
		Retention: config.RetentionConfig{IntervalMinutes: 60, IdempotencyHours: 48, InboxHours: 336, SessionHours: 720},
		Auth: config.AuthConfig{
			FirstPartyEnabled:     true,
			JWTSigningKey:         strings.Repeat("k", 40),
			AccessTokenTTLMinutes: 15,
			RefreshTokenTTLHours:  720,
		},
	}
}

func TestConfig_Validate(t *testing.T) {
	tests := []struct {
		name    string
		mutate  func(*config.Config)
		wantErr string // substring; empty means valid
	}{
		{"valid development", func(*config.Config) {}, ""},
		{"unknown env", func(c *config.Config) { c.Env = "prod" }, "AEDST_ENV"},
		{"short signing key", func(c *config.Config) { c.Auth.JWTSigningKey = "short" }, "AEDST_JWT_SIGNING_KEY"},
		{"dev key in production", func(c *config.Config) { c.Env = config.EnvProduction; c.Auth.JWTSigningKey = config.DevSigningKey }, "development placeholder"},
		{"wildcard cors in production", func(c *config.Config) { c.Env = config.EnvProduction; c.HTTP.AllowedOrigins = []string{"*"} }, "CORS"},
		{"min conns above max", func(c *config.Config) { c.DB.MinConns = 50 }, "MIN_CONNS"},
		{"jobs pool of one would deadlock the relay", func(c *config.Config) { c.DB.JobsMaxConns = 1 }, "JOBS_MAX_CONNS"},
		{"outbox batch must be positive", func(c *config.Config) { c.Outbox.BatchSize = 0 }, "AEDST_OUTBOX"},
		{"outbox concurrency must be positive", func(c *config.Config) { c.Outbox.Concurrency = 0 }, "AEDST_OUTBOX"},
		{"retention values must be positive", func(c *config.Config) { c.Retention.SessionHours = 0 }, "AEDST_RETENTION"},
		{"inbox must outlive the outbox", func(c *config.Config) { c.Retention.InboxHours = 24 }, "INBOX_HOURS"},
		{"negative api rate limit", func(c *config.Config) { c.HTTP.APIRateLimitPerMinute = -1 }, "AEDST_API_RATE_LIMIT"},
		{"valid min client versions", func(c *config.Config) {
			c.Client.MinVersions = map[string]string{"android": "1.2.0", "web": "2.0.0-rc.1"}
		}, ""},
		{"unknown client platform", func(c *config.Config) { c.Client.MinVersions = map[string]string{"toaster": "1.0.0"} }, "unknown platform"},
		{"malformed client version", func(c *config.Config) { c.Client.MinVersions = map[string]string{"android": "latest"} }, "semantic version"},
		{"no auth at all", func(c *config.Config) { c.Auth.FirstPartyEnabled = false }, "no authentication"},
		{"oidc without audience", func(c *config.Config) { c.OIDC.Issuer = "https://idp.example" }, "AUDIENCE"},
		{"oidc http in production", func(c *config.Config) {
			c.Env = config.EnvProduction
			c.Auth.JWTSigningKey = strings.Repeat("z", 40)
			c.OIDC = config.OIDCConfig{Issuer: "http://idp.example", Audience: "api", Algorithms: []string{"RS256"}}
		}, "https"},
		{"oidc symmetric algorithm", func(c *config.Config) {
			c.OIDC = config.OIDCConfig{Issuer: "https://idp.example", Audience: "api", Algorithms: []string{"HS256"}}
		}, "asymmetric"},
		{"oidc only, no first party", func(c *config.Config) {
			c.Auth.FirstPartyEnabled = false
			c.OIDC = config.OIDCConfig{Issuer: "https://idp.example", Audience: "api", Algorithms: []string{"RS256"}}
		}, ""},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			cfg := validConfig()
			tt.mutate(&cfg)
			err := cfg.Validate()
			switch {
			case tt.wantErr == "" && err != nil:
				t.Fatalf("Validate() = %v, want nil", err)
			case tt.wantErr != "" && (err == nil || !strings.Contains(err.Error(), tt.wantErr)):
				t.Fatalf("Validate() = %v, want error containing %q", err, tt.wantErr)
			}
		})
	}
}

// AEDST_MIN_CLIENT_VERSIONS is a comma-separated platform=version list; this
// pins the wire format the Helm chart and .env.example document.
func TestLoad_ParsesMinClientVersionsAndOutboxDefaults(t *testing.T) {
	t.Setenv("AEDST_DATABASE_URL", "postgres://x")
	t.Setenv("AEDST_JWT_SIGNING_KEY", strings.Repeat("k", 40))
	t.Setenv("AEDST_MIN_CLIENT_VERSIONS", "web=1.0.0, android=1.2.0")

	cfg, err := config.Load()
	if err != nil {
		t.Fatalf("Load() error = %v", err)
	}
	if got := cfg.Client.MinVersions; len(got) != 2 || got["web"] != "1.0.0" || got["android"] != "1.2.0" {
		t.Errorf("MinVersions = %v, want web=1.0.0 android=1.2.0", got)
	}
	if o := cfg.Outbox; o.RelayIntervalMs != 500 || o.BatchSize != 100 || o.Concurrency != 4 || o.RetentionHours != 168 {
		t.Errorf("Outbox defaults = %+v", o)
	}
	if cfg.DB.JobsMaxConns != 10 || cfg.DB.DirectDSN != "" {
		t.Errorf("DB jobs defaults = %d / %q, want 10 / empty", cfg.DB.JobsMaxConns, cfg.DB.DirectDSN)
	}
}

func TestLoad_EmptyMinClientVersionsDisablesTheGate(t *testing.T) {
	t.Setenv("AEDST_DATABASE_URL", "postgres://x")
	t.Setenv("AEDST_JWT_SIGNING_KEY", strings.Repeat("k", 40))
	cfg, err := config.Load()
	if err != nil {
		t.Fatalf("Load() error = %v", err)
	}
	if len(cfg.Client.MinVersions) != 0 {
		t.Errorf("MinVersions = %v, want none", cfg.Client.MinVersions)
	}
}

func TestConfig_ValidateFeatures(t *testing.T) {
	tests := []struct {
		name    string
		mutate  func(*config.Config)
		wantErr string
	}{
		{"write timeout must exceed request timeout", func(c *config.Config) { c.Server.WriteTimeoutSeconds = 30 }, "AEDST_HTTP_WRITE_TIMEOUT_SECONDS"},
		{"tls cert without key", func(c *config.Config) { c.Server.TLSCertFile = "cert.pem" }, "AEDST_TLS_CERT_FILE"},
		{"origin with a path", func(c *config.Config) { c.HTTP.AllowedOrigins = []string{"https://app.example.com/x"} }, "not an origin"},
		{"unknown queue", func(c *config.Config) { c.Jobs.Queues = map[string]int{"emails": 1} }, "unknown queue"},
		{"zero workers", func(c *config.Config) { c.Jobs.Queues = map[string]int{"media": 0} }, "positive worker count"},
		{"bad storage provider", func(c *config.Config) {
			c.Storage = config.StorageConfig{Bucket: "b", Provider: "gcs", UploadURLTTLSeconds: 1, DownloadURLTTLSeconds: 1}
		}, "AEDST_STORAGE_PROVIDER"},
		{"vapid without subject", func(c *config.Config) { c.Push.VAPIDPrivateKey = "k" }, "AEDST_PUSH_VAPID_SUBJECT"},
		{"valid queues and storage", func(c *config.Config) {
			c.Jobs.Queues = map[string]int{"default": 10, "media": 2}
			c.Storage = config.StorageConfig{Bucket: "b", Provider: "minio", UploadURLTTLSeconds: 1, DownloadURLTTLSeconds: 1}
		}, ""},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			cfg := validConfig()
			tt.mutate(&cfg)
			err := cfg.Validate()
			if tt.wantErr == "" {
				if err != nil {
					t.Fatalf("Validate() = %v, want nil", err)
				}
				return
			}
			if err == nil || !strings.Contains(err.Error(), tt.wantErr) {
				t.Fatalf("Validate() = %v, want error containing %q", err, tt.wantErr)
			}
		})
	}
}
