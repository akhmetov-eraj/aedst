// Package config loads process configuration from the environment. All
// configuration is environment-driven (12-factor) so the same binary/image
// runs unchanged in development, test, staging and production — see
// docs/content/deployment.md.
//
// Load parses AND validates: the process refuses to start on a missing,
// malformed or unsafe value (e.g. the development signing key in production)
// instead of failing later under load.
//
// OpenTelemetry is configured through the standard OTEL_* environment
// variables (OTEL_EXPORTER_OTLP_ENDPOINT, OTEL_EXPORTER_OTLP_HEADERS,
// OTEL_EXPORTER_OTLP_PROTOCOL, OTEL_RESOURCE_ATTRIBUTES, ...) read directly by
// the SDK, so any OTLP backend works without code changes.
package config

import (
	"errors"
	"fmt"
	"net/url"
	"slices"
	"strings"

	"github.com/caarlos0/env/v11"
	"golang.org/x/mod/semver"
)

// Env identifies the deployment environment.
type Env string

const (
	EnvDevelopment Env = "development"
	EnvTest        Env = "test"
	EnvStaging     Env = "staging"
	EnvProduction  Env = "production"
)

// IsDeployed reports whether e is a shared, non-local environment where
// development defaults must never be accepted.
func (e Env) IsDeployed() bool { return e == EnvStaging || e == EnvProduction }

// DevSigningKey is the placeholder key shipped in .env.example. Load rejects
// it in staging/production.
const DevSigningKey = "dev-only-signing-key-change-me-0123456789"

// minSigningKeyBytes is the RFC 7518 §3.2 minimum for HS256 keys.
const minSigningKeyBytes = 32

// Config is the full process configuration. Fail-fast semantics: Load returns
// an error rather than starting the process with a missing required value.
type Config struct {
	Env Env `env:"AEDST_ENV" envDefault:"development"`

	HTTP      HTTPConfig
	DB        DatabaseConfig
	Valkey    ValkeyConfig
	Auth      AuthConfig
	OIDC      OIDCConfig
	Obs       ObservabilityConfig
	Outbox    OutboxConfig
	Client    ClientConfig
	Retention RetentionConfig
	Server    ServerConfig
	Jobs      JobsConfig
	Storage   StorageConfig
	Push      PushConfig
	Media     MediaConfig
	Tasks     TasksConfig
}

type HTTPConfig struct {
	Addr string `env:"AEDST_HTTP_ADDR" envDefault:":8080"`
	// MetricsAddr is the listen address of the optional Prometheus scrape
	// endpoint (/metrics). Empty disables it; OTLP push is unaffected.
	MetricsAddr     string `env:"AEDST_METRICS_ADDR" envDefault:""`
	ShutdownTimeout int    `env:"AEDST_SHUTDOWN_TIMEOUT_SECONDS" envDefault:"15"`
	// ShutdownDrainSeconds is how long /readyz reports not-ready before the
	// listener stops accepting, so a load balancer can drain the instance.
	// Set to the LB's health-check interval x threshold in production.
	ShutdownDrainSeconds int `env:"AEDST_SHUTDOWN_DRAIN_SECONDS" envDefault:"0"`
	RequestTimeout       int `env:"AEDST_REQUEST_TIMEOUT_SECONDS" envDefault:"30"`
	// MaxRequestBytes caps a single RPC request message.
	MaxRequestBytes int `env:"AEDST_MAX_REQUEST_BYTES" envDefault:"1048576"`
	// AllowedOrigins lists the browser origins (e.g. apps/web in local dev)
	// permitted to call the ConnectRPC API cross-origin. Connect's protocol is
	// plain HTTP/JSON so browsers can call it directly (see
	// docs/content/decisions/0002-connectrpc.md), but that still requires CORS.
	AllowedOrigins []string `env:"AEDST_CORS_ALLOWED_ORIGINS" envSeparator:"," envDefault:"http://localhost:3000"`
	// TrustedProxyHops is the exact number of reverse proxies between the
	// public internet and this server (Cloudflare -> ALB = 2). 0 means the
	// socket peer is the client. Wrong values let clients spoof the IP used for
	// rate limiting, so verify after deploying (see chi ClientIPFromXFFTrustedProxies).
	TrustedProxyHops int `env:"AEDST_TRUSTED_PROXY_HOPS" envDefault:"0"`
	// AuthRateLimitPerMinute is the per-client-IP budget for the public
	// authentication RPCs (register/login/refresh). 0 disables.
	AuthRateLimitPerMinute int `env:"AEDST_AUTH_RATE_LIMIT_PER_MINUTE" envDefault:"30"`
	// APIRateLimitPerMinute is the per-user budget for authenticated RPCs,
	// shared by every replica through Valkey. It bounds what one account (or one
	// leaked token) can take from everyone else. 0 disables.
	APIRateLimitPerMinute int `env:"AEDST_API_RATE_LIMIT_PER_MINUTE" envDefault:"1200"`
	// MaxInFlight caps concurrently processing requests per instance; the excess
	// is shed with 429 + Retry-After rather than queued until latency collapses.
	// 0 disables.
	MaxInFlight int `env:"AEDST_MAX_IN_FLIGHT" envDefault:"1024"`
	// GRPCReflectionEnabled serves gRPC server reflection (schema discovery for
	// grpcurl and similar). Off by default: it publishes the whole API surface
	// to anyone who can reach the port.
	GRPCReflectionEnabled bool `env:"AEDST_GRPC_REFLECTION_ENABLED" envDefault:"false"`
}

type DatabaseConfig struct {
	DSN      string `env:"AEDST_DATABASE_URL,required"`
	MaxConns int32  `env:"AEDST_DATABASE_MAX_CONNS" envDefault:"20"`
	MinConns int32  `env:"AEDST_DATABASE_MIN_CONNS" envDefault:"2"`
	// MaxConnLifetimeMinutes recycles connections so a failover/DNS change is
	// picked up and server-side memory does not grow unbounded.
	MaxConnLifetimeMinutes int `env:"AEDST_DATABASE_MAX_CONN_LIFETIME_MINUTES" envDefault:"60"`
	MaxConnIdleMinutes     int `env:"AEDST_DATABASE_MAX_CONN_IDLE_MINUTES" envDefault:"15"`
	// StatementTimeoutMs aborts runaway queries server-side (0 = no limit).
	StatementTimeoutMs int `env:"AEDST_DATABASE_STATEMENT_TIMEOUT_MS" envDefault:"15000"`
	// IdleInTxTimeoutMs kills sessions that hold a transaction open while
	// idle (0 = no limit).
	IdleInTxTimeoutMs int `env:"AEDST_DATABASE_IDLE_IN_TX_TIMEOUT_MS" envDefault:"30000"`

	// JobsMaxConns sizes the second pool used by background work (River,
	// outbox relay, event consumers). Keeping it apart from MaxConns means a
	// burst of jobs can only exhaust its own connections, never the ones
	// request handlers need — and a slow request storm cannot starve the
	// relay either. Budget: replicas x (MaxConns + JobsMaxConns) must stay
	// below Postgres max_connections (or PgBouncer's server pool).
	JobsMaxConns int32 `env:"AEDST_DATABASE_JOBS_MAX_CONNS" envDefault:"10"`
	// DirectDSN, when set, is used by the jobs pool instead of DSN. Point DSN
	// at PgBouncer (transaction pooling) for request traffic and DirectDSN at
	// Postgres itself: River uses LISTEN/NOTIFY and platform/lock uses
	// session-level advisory locks, neither of which survives transaction
	// pooling.
	DirectDSN string `env:"AEDST_DATABASE_DIRECT_DSN" envDefault:""`
}

// ValkeyConfig configures the Valkey (cache + rate limiting) connection. It
// replaces the former AEDST_REDIS_* variables; the go-redis client speaks the
// same protocol.
type ValkeyConfig struct {
	Addr     string `env:"AEDST_VALKEY_ADDR" envDefault:"localhost:6379"`
	Password string `env:"AEDST_VALKEY_PASSWORD" envDefault:""`
	DB       int    `env:"AEDST_VALKEY_DB" envDefault:"0"`
}

// AuthConfig configures the built-in first-party identity provider (email +
// password sessions). Disable it when an external OIDC provider (see
// OIDCConfig) is the system of record for identities.
type AuthConfig struct {
	FirstPartyEnabled bool `env:"AEDST_AUTH_FIRST_PARTY_ENABLED" envDefault:"true"`
	// JWTSigningKey is the HS256 secret for first-party tokens (>= 32 bytes).
	// Rotation: move the old key to JWTPreviousSigningKey (verify-only).
	JWTSigningKey         string `env:"AEDST_JWT_SIGNING_KEY"`
	JWTPreviousSigningKey string `env:"AEDST_JWT_PREVIOUS_SIGNING_KEY" envDefault:""`
	AccessTokenTTLMinutes int    `env:"AEDST_ACCESS_TOKEN_TTL_MINUTES" envDefault:"15"`
	RefreshTokenTTLHours  int    `env:"AEDST_REFRESH_TOKEN_TTL_HOURS" envDefault:"720"`
	// LoginRateLimitPerMinute is the per-account budget for password
	// attempts (independent of the per-IP limit). 0 disables.
	LoginRateLimitPerMinute int `env:"AEDST_LOGIN_RATE_LIMIT_PER_MINUTE" envDefault:"10"`
	// RolePolicy restricts procedures to callers holding one of the listed roles:
	// "/pkg.Service/Method=role|role,/pkg.Other/Method=role". A procedure that is
	// unknown or public is a startup error (see httpserver.ValidateRolePolicy), so
	// a typo cannot leave the real RPC open. Empty means every authenticated caller
	// may call every RPC.
	RolePolicy map[string]string `env:"AEDST_ROLE_POLICY" envSeparator:"," envKeyValSeparator:"=" envDefault:""`
}

// OIDCConfig makes the server an OIDC resource server: it accepts access
// tokens issued by any standards-compliant provider (Zitadel, Keycloak,
// Ory Hydra, Auth0, ...). Nothing here is provider-specific. Leave Issuer
// empty to accept first-party tokens only.
type OIDCConfig struct {
	// Issuer is the provider's issuer URL; discovery is fetched from
	// <issuer>/.well-known/openid-configuration.
	Issuer string `env:"AEDST_OIDC_ISSUER" envDefault:""`
	// Audience is the value that must appear in the token's aud claim
	// (typically the API's client id / project id).
	Audience string `env:"AEDST_OIDC_AUDIENCE" envDefault:""`
	// JWKSURL overrides the jwks_uri found in discovery (rarely needed).
	JWKSURL string `env:"AEDST_OIDC_JWKS_URL" envDefault:""`
	// Algorithms is the allow-list of accepted JWS algorithms. Asymmetric
	// only: HS* is refused for external issuers (algorithm-confusion guard).
	Algorithms []string `env:"AEDST_OIDC_ALGORITHMS" envSeparator:"," envDefault:"RS256,ES256,EdDSA"`
	// RolesClaim locates roles in the token: a top-level claim name or a
	// dotted path (Keycloak: "realm_access.roles"). An array of strings or an
	// object keyed by role name (Zitadel) are both understood.
	RolesClaim       string `env:"AEDST_OIDC_ROLES_CLAIM" envDefault:"roles"`
	ClockSkewSeconds int    `env:"AEDST_OIDC_CLOCK_SKEW_SECONDS" envDefault:"30"`
}

type ObservabilityConfig struct {
	LogLevel    string `env:"AEDST_LOG_LEVEL" envDefault:"info"`
	LogFormat   string `env:"AEDST_LOG_FORMAT" envDefault:"json"`
	ServiceName string `env:"AEDST_SERVICE_NAME" envDefault:"aedst-server"`
	// OTLPEndpoint / OTLPTracesEndpoint mirror the standard OTEL_* variables
	// only so startup can tell whether telemetry export is configured at all;
	// the SDK reads the real values (and headers, protocol, ...) itself.
	OTLPEndpoint       string `env:"OTEL_EXPORTER_OTLP_ENDPOINT" envDefault:""`
	OTLPTracesEndpoint string `env:"OTEL_EXPORTER_OTLP_TRACES_ENDPOINT" envDefault:""`
}

// ExportEnabled reports whether any OTLP endpoint is configured.
func (o ObservabilityConfig) ExportEnabled() bool {
	return o.OTLPEndpoint != "" || o.OTLPTracesEndpoint != ""
}

// Load parses configuration from the process environment and validates it.
func Load() (Config, error) {
	var cfg Config
	if err := env.Parse(&cfg); err != nil {
		return Config{}, fmt.Errorf("config: %w", err)
	}
	cfg.Client.MinVersions = normalizeMinVersions(cfg.Client.MinVersions)
	if err := cfg.Validate(); err != nil {
		return Config{}, fmt.Errorf("config: %w", err)
	}
	return cfg, nil
}

// Validate checks cross-field and environment-specific invariants, returning
// every problem at once.
func (c Config) Validate() error {
	var errs []error
	bad := func(format string, args ...any) { errs = append(errs, fmt.Errorf(format, args...)) }

	switch c.Env {
	case EnvDevelopment, EnvTest, EnvStaging, EnvProduction:
	default:
		bad("AEDST_ENV %q must be one of development, test, staging, production", c.Env)
	}

	if c.DB.MinConns > c.DB.MaxConns {
		bad("AEDST_DATABASE_MIN_CONNS (%d) must not exceed AEDST_DATABASE_MAX_CONNS (%d)", c.DB.MinConns, c.DB.MaxConns)
	}
	if c.DB.JobsMaxConns < 2 {
		// One connection is held by a relay pass while its subscribers need
		// another; a pool of one would deadlock event delivery.
		bad("AEDST_DATABASE_JOBS_MAX_CONNS (%d) must be at least 2", c.DB.JobsMaxConns)
	}
	if c.Outbox.RelayIntervalMs <= 0 || c.Outbox.BatchSize <= 0 || c.Outbox.Concurrency <= 0 || c.Outbox.RetentionHours <= 0 {
		bad("AEDST_OUTBOX_RELAY_INTERVAL_MS, AEDST_OUTBOX_BATCH_SIZE, AEDST_OUTBOX_CONCURRENCY and AEDST_OUTBOX_RETENTION_HOURS must be positive")
	}
	if c.Retention.IntervalMinutes <= 0 || c.Retention.IdempotencyHours <= 0 || c.Retention.InboxHours <= 0 || c.Retention.SessionHours <= 0 {
		bad("AEDST_RETENTION_* values must be positive")
	}
	if c.Retention.InboxHours < c.Outbox.RetentionHours {
		// A processed-event marker must outlive the outbox row it dedupes, or a
		// late redelivery of a still-published event would be applied again.
		bad("AEDST_RETENTION_INBOX_HOURS (%d) must be at least AEDST_OUTBOX_RETENTION_HOURS (%d)", c.Retention.InboxHours, c.Outbox.RetentionHours)
	}
	if c.HTTP.APIRateLimitPerMinute < 0 || c.HTTP.MaxInFlight < 0 {
		bad("AEDST_API_RATE_LIMIT_PER_MINUTE and AEDST_MAX_IN_FLIGHT must not be negative")
	}
	for platform, version := range c.Client.MinVersions {
		if !slices.Contains(ClientPlatforms, platform) {
			bad("AEDST_MIN_CLIENT_VERSIONS: unknown platform %q (want one of %v)", platform, ClientPlatforms)
		}
		if !semver.IsValid("v" + version) {
			bad("AEDST_MIN_CLIENT_VERSIONS: %q is not a semantic version for %s", version, platform)
		}
	}
	if c.HTTP.RequestTimeout <= 0 {
		bad("AEDST_REQUEST_TIMEOUT_SECONDS must be positive")
	}
	if c.HTTP.MaxRequestBytes <= 0 {
		bad("AEDST_MAX_REQUEST_BYTES must be positive")
	}
	if c.Env.IsDeployed() && slices.Contains(c.HTTP.AllowedOrigins, "*") {
		bad("AEDST_CORS_ALLOWED_ORIGINS must not contain \"*\" in %s", c.Env)
	}

	if c.Auth.FirstPartyEnabled {
		if len(c.Auth.JWTSigningKey) < minSigningKeyBytes {
			bad("AEDST_JWT_SIGNING_KEY must be at least %d bytes (generate one with: openssl rand -base64 48)", minSigningKeyBytes)
		}
		if c.Auth.JWTPreviousSigningKey != "" && len(c.Auth.JWTPreviousSigningKey) < minSigningKeyBytes {
			bad("AEDST_JWT_PREVIOUS_SIGNING_KEY must be at least %d bytes", minSigningKeyBytes)
		}
		if c.Env.IsDeployed() && c.Auth.JWTSigningKey == DevSigningKey {
			bad("AEDST_JWT_SIGNING_KEY is the development placeholder; set a real secret in %s", c.Env)
		}
		if c.Auth.AccessTokenTTLMinutes <= 0 || c.Auth.RefreshTokenTTLHours <= 0 {
			bad("token TTLs must be positive")
		}
	}

	if c.OIDC.Issuer != "" {
		u, err := url.Parse(c.OIDC.Issuer)
		switch {
		case err != nil || u.Host == "":
			bad("AEDST_OIDC_ISSUER %q is not a valid URL", c.OIDC.Issuer)
		case u.Scheme != "https" && (u.Scheme != "http" || c.Env.IsDeployed()):
			bad("AEDST_OIDC_ISSUER must use https (http is allowed only outside staging/production)")
		}
		if c.OIDC.Audience == "" {
			bad("AEDST_OIDC_AUDIENCE is required when AEDST_OIDC_ISSUER is set (audience validation must not be skipped)")
		}
		for _, alg := range c.OIDC.Algorithms {
			if strings.HasPrefix(strings.ToUpper(alg), "HS") || strings.EqualFold(alg, "none") {
				bad("AEDST_OIDC_ALGORITHMS must not include %q (asymmetric algorithms only)", alg)
			}
		}
	}
	c.validateFeatures(bad)

	if !c.Auth.FirstPartyEnabled && c.OIDC.Issuer == "" {
		bad("no authentication configured: enable AEDST_AUTH_FIRST_PARTY_ENABLED or set AEDST_OIDC_ISSUER")
	}

	return errors.Join(errs...)
}

// OutboxConfig tunes the transactional-outbox relay (see platform/outbox).
type OutboxConfig struct {
	// RelayIntervalMs is how often an idle relay polls for new events; it is
	// the latency floor of every integration event.
	RelayIntervalMs int `env:"AEDST_OUTBOX_RELAY_INTERVAL_MS" envDefault:"500"`
	// BatchSize is the most events one relay pass claims and delivers.
	BatchSize int `env:"AEDST_OUTBOX_BATCH_SIZE" envDefault:"100"`
	// Concurrency is how many events of a batch are published in parallel.
	Concurrency int `env:"AEDST_OUTBOX_CONCURRENCY" envDefault:"4"`
	// RetentionHours is how long delivered events are kept before purge.
	RetentionHours int `env:"AEDST_OUTBOX_RETENTION_HOURS" envDefault:"168"`
}

// ClientConfig configures client-version enforcement (see
// docs/content/decisions/0009-api-compatibility-and-client-versioning.md).
type ClientConfig struct {
	// MinVersions maps a client platform (web, android, apple, desktop) to
	// the oldest release the server still supports, e.g.
	// "android=1.2.0,apple=1.2.0". Requests from an older, self-identified
	// client are rejected with CLIENT_UPGRADE_REQUIRED. Empty disables the
	// gate; a request that does not identify itself is never rejected.
	MinVersions map[string]string `env:"AEDST_MIN_CLIENT_VERSIONS" envSeparator:"," envKeyValSeparator:"=" envDefault:""`
}

// ClientPlatforms are the values a client may send as the platform half of
// the X-Aedst-Client header.
var ClientPlatforms = []string{"web", "android", "apple", "desktop"}

// normalizeMinVersions trims whitespace and lower-cases platform names so
// "web=1.0.0, Android=1.2.0" (a natural way to write the list) means what it
// looks like instead of failing as an unknown platform " Android".
func normalizeMinVersions(in map[string]string) map[string]string {
	if len(in) == 0 {
		return in
	}
	out := make(map[string]string, len(in))
	for platform, version := range in {
		out[strings.ToLower(strings.TrimSpace(platform))] = strings.TrimSpace(version)
	}
	return out
}

// RetentionConfig bounds the append-only tables (see internal/platform/retention).
type RetentionConfig struct {
	// IntervalMinutes is how often each retention policy runs.
	IntervalMinutes int `env:"AEDST_RETENTION_INTERVAL_MINUTES" envDefault:"60"`
	// IdempotencyHours is how long an idempotency key is remembered: it only has
	// to outlive a client's retry window.
	IdempotencyHours int `env:"AEDST_RETENTION_IDEMPOTENCY_HOURS" envDefault:"48"`
	// InboxHours is how long processed-event markers are kept. It must not be
	// shorter than AEDST_OUTBOX_RETENTION_HOURS, or an event redelivered late
	// could be applied twice once its marker is gone.
	InboxHours int `env:"AEDST_RETENTION_INBOX_HOURS" envDefault:"336"`
	// SessionHours is how long an expired or revoked session row is kept (for
	// audit and support) before deletion.
	SessionHours int `env:"AEDST_RETENTION_SESSION_HOURS" envDefault:"720"`
	// NotificationDays is how long in-app notifications are kept.
	NotificationDays int `env:"AEDST_RETENTION_NOTIFICATION_DAYS" envDefault:"90"`
	// AuditDays is how long admin audit entries are kept.
	AuditDays int `env:"AEDST_RETENTION_AUDIT_DAYS" envDefault:"365"`
}
