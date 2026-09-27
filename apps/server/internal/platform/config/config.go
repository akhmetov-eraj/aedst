// Package config loads process configuration from the environment. All
// configuration is environment-driven (12-factor) so the same binary/image
// runs unchanged in development, test, staging and production — see
// docs/deployment.md.
package config

import (
	"fmt"

	"github.com/caarlos0/env/v11"
)

// Env identifies the deployment environment.
type Env string

const (
	EnvDevelopment Env = "development"
	EnvTest        Env = "test"
	EnvStaging     Env = "staging"
	EnvProduction  Env = "production"
)

// Config is the full process configuration. Fail-fast semantics: Load returns
// an error rather than starting the process with a missing required value.
type Config struct {
	Env Env `env:"AEDST_ENV" envDefault:"development"`

	HTTP  HTTPConfig
	DB    DatabaseConfig
	Redis RedisConfig
	NATS  NATSConfig
	Auth  AuthConfig
	Obs   ObservabilityConfig
}

type HTTPConfig struct {
	Addr            string `env:"AEDST_HTTP_ADDR" envDefault:":8080"`
	MetricsAddr     string `env:"AEDST_METRICS_ADDR" envDefault:":9090"`
	ShutdownTimeout int    `env:"AEDST_SHUTDOWN_TIMEOUT_SECONDS" envDefault:"15"`
	// AllowedOrigins lists the browser origins (e.g. apps/web in local dev)
	// permitted to call the ConnectRPC API cross-origin. Connect's protocol is
	// plain HTTP/JSON so browsers can call it directly (see
	// docs/decisions/0002-connectrpc.md), but that still requires CORS.
	AllowedOrigins []string `env:"AEDST_CORS_ALLOWED_ORIGINS" envSeparator:"," envDefault:"http://localhost:3000"`
}

type DatabaseConfig struct {
	DSN      string `env:"AEDST_DATABASE_URL,required"`
	MaxConns int32  `env:"AEDST_DATABASE_MAX_CONNS" envDefault:"20"`
	MinConns int32  `env:"AEDST_DATABASE_MIN_CONNS" envDefault:"2"`
}

type RedisConfig struct {
	Addr     string `env:"AEDST_REDIS_ADDR" envDefault:"localhost:6379"`
	Password string `env:"AEDST_REDIS_PASSWORD" envDefault:""`
	DB       int    `env:"AEDST_REDIS_DB" envDefault:"0"`
}

type NATSConfig struct {
	URL string `env:"AEDST_NATS_URL" envDefault:"nats://localhost:4222"`
}

type AuthConfig struct {
	// JWTSigningKey is the current HMAC/EdDSA signing key material (PEM or raw
	// secret, depending on the configured algorithm). Rotation is handled by
	// platform/auth accepting a set of verification keys; see its package doc.
	JWTSigningKey         string `env:"AEDST_JWT_SIGNING_KEY,required"`
	JWTPreviousSigningKey string `env:"AEDST_JWT_PREVIOUS_SIGNING_KEY" envDefault:""`
	AccessTokenTTLMinutes int    `env:"AEDST_ACCESS_TOKEN_TTL_MINUTES" envDefault:"15"`
	RefreshTokenTTLHours  int    `env:"AEDST_REFRESH_TOKEN_TTL_HOURS" envDefault:"720"`
}

type ObservabilityConfig struct {
	LogLevel     string `env:"AEDST_LOG_LEVEL" envDefault:"info"`
	LogFormat    string `env:"AEDST_LOG_FORMAT" envDefault:"json"`
	OTLPEndpoint string `env:"AEDST_OTLP_ENDPOINT" envDefault:""`
	ServiceName  string `env:"AEDST_SERVICE_NAME" envDefault:"aedst-server"`
}

// Load parses configuration from the process environment.
func Load() (Config, error) {
	var cfg Config
	if err := env.Parse(&cfg); err != nil {
		return Config{}, fmt.Errorf("config: %w", err)
	}
	return cfg, nil
}
