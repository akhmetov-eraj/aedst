package config

import (
	"net/url"
	"slices"
	"strings"
)

// ServerConfig bounds the HTTP listener (see httpserver.ServerConfig). Zero
// means "use the built-in default".
type ServerConfig struct {
	ReadHeaderTimeoutSeconds int `env:"AEDST_HTTP_READ_HEADER_TIMEOUT_SECONDS" envDefault:"5"`
	ReadTimeoutSeconds       int `env:"AEDST_HTTP_READ_TIMEOUT_SECONDS" envDefault:"30"`
	// WriteTimeoutSeconds must exceed AEDST_REQUEST_TIMEOUT_SECONDS so a
	// timed-out RPC can still be answered. Streaming RPCs clear it per request.
	WriteTimeoutSeconds int `env:"AEDST_HTTP_WRITE_TIMEOUT_SECONDS" envDefault:"60"`
	IdleTimeoutSeconds  int `env:"AEDST_HTTP_IDLE_TIMEOUT_SECONDS" envDefault:"120"`
	MaxHeaderBytes      int `env:"AEDST_HTTP_MAX_HEADER_BYTES" envDefault:"65536"`
	// TLSCertFile/TLSKeyFile enable in-process TLS (min TLS 1.2). The default
	// deployment terminates TLS at the ingress/load balancer instead.
	TLSCertFile string `env:"AEDST_TLS_CERT_FILE" envDefault:""`
	TLSKeyFile  string `env:"AEDST_TLS_KEY_FILE" envDefault:""`
	// HSTSMaxAgeSeconds is sent as Strict-Transport-Security on HTTPS
	// requests (directly or via a trusted proxy's X-Forwarded-Proto). 0 disables.
	HSTSMaxAgeSeconds int `env:"AEDST_HSTS_MAX_AGE_SECONDS" envDefault:"63072000"`
	// RequireConnectProtocolHeader rejects Connect unary requests without the
	// Connect-Protocol-Version header (CSRF defense in depth; every official
	// Connect client sends it).
	RequireConnectProtocolHeader bool `env:"AEDST_REQUIRE_CONNECT_PROTOCOL_HEADER" envDefault:"true"`
}

// JobsConfig selects which River queues this process works.
type JobsConfig struct {
	// Queues maps queue -> max concurrent workers, e.g. "default=10,media=2".
	// A dedicated media worker deployment runs "media=4" and API replicas
	// "default=10"; an empty value makes the process insert-only.
	Queues map[string]int `env:"AEDST_JOBS_QUEUES" envSeparator:"," envKeyValSeparator:"=" envDefault:"default=10,media=2"`
}

// KnownQueues are the River queues the server defines.
var KnownQueues = []string{"default", "media"}

// StorageConfig configures object storage (see platform/storage). Empty
// Bucket disables media uploads (MediaService answers FAILED_PRECONDITION).
type StorageConfig struct {
	Provider        string `env:"AEDST_STORAGE_PROVIDER" envDefault:"s3"`
	Endpoint        string `env:"AEDST_STORAGE_ENDPOINT" envDefault:""`
	PublicEndpoint  string `env:"AEDST_STORAGE_PUBLIC_ENDPOINT" envDefault:""`
	Region          string `env:"AEDST_STORAGE_REGION" envDefault:""`
	Bucket          string `env:"AEDST_STORAGE_BUCKET" envDefault:""`
	AccessKeyID     string `env:"AEDST_STORAGE_ACCESS_KEY_ID" envDefault:""`
	SecretAccessKey string `env:"AEDST_STORAGE_SECRET_ACCESS_KEY" envDefault:""`
	PathStyle       string `env:"AEDST_STORAGE_PATH_STYLE" envDefault:""`
	// UploadURLTTLSeconds / DownloadURLTTLSeconds bound presigned URLs.
	UploadURLTTLSeconds   int `env:"AEDST_STORAGE_UPLOAD_URL_TTL_SECONDS" envDefault:"900"`
	DownloadURLTTLSeconds int `env:"AEDST_STORAGE_DOWNLOAD_URL_TTL_SECONDS" envDefault:"3600"`
}

// PushConfig configures the push providers (see platform/push). Each one is
// disabled while its required values are empty.
type PushConfig struct {
	VAPIDPublicKey  string `env:"AEDST_PUSH_VAPID_PUBLIC_KEY" envDefault:""`
	VAPIDPrivateKey string `env:"AEDST_PUSH_VAPID_PRIVATE_KEY" envDefault:""`
	VAPIDSubject    string `env:"AEDST_PUSH_VAPID_SUBJECT" envDefault:""`

	FCMCredentialsJSON string `env:"AEDST_PUSH_FCM_CREDENTIALS_JSON" envDefault:""`
	FCMCredentialsFile string `env:"AEDST_PUSH_FCM_CREDENTIALS_FILE" envDefault:""`
	FCMProjectID       string `env:"AEDST_PUSH_FCM_PROJECT_ID" envDefault:""`

	APNsKeyID          string `env:"AEDST_PUSH_APNS_KEY_ID" envDefault:""`
	APNsTeamID         string `env:"AEDST_PUSH_APNS_TEAM_ID" envDefault:""`
	APNsPrivateKey     string `env:"AEDST_PUSH_APNS_PRIVATE_KEY" envDefault:""`
	APNsPrivateKeyFile string `env:"AEDST_PUSH_APNS_PRIVATE_KEY_FILE" envDefault:""`
	APNsTopic          string `env:"AEDST_PUSH_APNS_TOPIC" envDefault:""`

	TimeoutSeconds int `env:"AEDST_PUSH_TIMEOUT_SECONDS" envDefault:"10"`
}

func (p PushConfig) WebPushEnabled() bool { return p.VAPIDPrivateKey != "" }
func (p PushConfig) FCMEnabled() bool {
	return p.FCMCredentialsJSON != "" || p.FCMCredentialsFile != ""
}

func (p PushConfig) APNsEnabled() bool {
	return p.APNsKeyID != "" && (p.APNsPrivateKey != "" || p.APNsPrivateKeyFile != "")
}

// MediaConfig bounds uploads and processing.
type MediaConfig struct {
	MaxImageBytes    int64 `env:"AEDST_MEDIA_MAX_IMAGE_BYTES" envDefault:"20971520"`
	MaxVideoBytes    int64 `env:"AEDST_MEDIA_MAX_VIDEO_BYTES" envDefault:"524288000"`
	MaxAudioBytes    int64 `env:"AEDST_MEDIA_MAX_AUDIO_BYTES" envDefault:"104857600"`
	MaxDocumentBytes int64 `env:"AEDST_MEDIA_MAX_DOCUMENT_BYTES" envDefault:"52428800"`
	// MaxImagePixels guards against decompression bombs.
	MaxImagePixels int64 `env:"AEDST_MEDIA_MAX_IMAGE_PIXELS" envDefault:"50000000"`
	// FFmpegPath / FFprobePath enable video posters and audio/video metadata.
	FFmpegPath  string `env:"AEDST_MEDIA_FFMPEG_PATH" envDefault:""`
	FFprobePath string `env:"AEDST_MEDIA_FFPROBE_PATH" envDefault:""`
	// ProcessTimeoutSeconds bounds one external ffmpeg/ffprobe invocation.
	ProcessTimeoutSeconds int `env:"AEDST_MEDIA_PROCESS_TIMEOUT_SECONDS" envDefault:"120"`
	// PendingUploadTTLHours is how long a PENDING_UPLOAD row (and any object
	// uploaded for it) lives before the retention job removes it.
	PendingUploadTTLHours int `env:"AEDST_MEDIA_PENDING_UPLOAD_TTL_HOURS" envDefault:"24"`
}

// TasksConfig tunes the tasks module.
type TasksConfig struct {
	// DueSoonLeadMinutes is how long before a task's due date TASK_DUE_SOON fires.
	DueSoonLeadMinutes int `env:"AEDST_TASKS_DUE_SOON_LEAD_MINUTES" envDefault:"60"`
}

func (c Config) validateFeatures(bad func(string, ...any)) {
	s := c.Server
	if s.ReadHeaderTimeoutSeconds < 0 || s.ReadTimeoutSeconds < 0 || s.WriteTimeoutSeconds < 0 || s.IdleTimeoutSeconds < 0 || s.MaxHeaderBytes < 0 || s.HSTSMaxAgeSeconds < 0 {
		bad("AEDST_HTTP_* timeouts/limits and AEDST_HSTS_MAX_AGE_SECONDS must not be negative")
	}
	if s.WriteTimeoutSeconds > 0 && s.WriteTimeoutSeconds <= c.HTTP.RequestTimeout {
		bad("AEDST_HTTP_WRITE_TIMEOUT_SECONDS (%d) must exceed AEDST_REQUEST_TIMEOUT_SECONDS (%d), or timed-out RPCs cannot be answered", s.WriteTimeoutSeconds, c.HTTP.RequestTimeout)
	}
	if (s.TLSCertFile == "") != (s.TLSKeyFile == "") {
		bad("AEDST_TLS_CERT_FILE and AEDST_TLS_KEY_FILE must be set together")
	}
	for _, origin := range c.HTTP.AllowedOrigins {
		if origin == "*" {
			continue // rejected in deployed environments above
		}
		u, err := url.Parse(origin)
		if err != nil || (u.Scheme != "http" && u.Scheme != "https") || u.Host == "" || (u.Path != "" && u.Path != "/") {
			bad("AEDST_CORS_ALLOWED_ORIGINS: %q is not an origin (scheme://host[:port])", origin)
		}
	}
	for queue, workers := range c.Jobs.Queues {
		if !slices.Contains(KnownQueues, queue) {
			bad("AEDST_JOBS_QUEUES: unknown queue %q (want one of %v)", queue, KnownQueues)
		}
		if workers <= 0 {
			bad("AEDST_JOBS_QUEUES: queue %q needs a positive worker count", queue)
		}
	}
	if c.Storage.Bucket != "" {
		switch c.Storage.Provider {
		case "s3", "minio", "r2":
		default:
			bad("AEDST_STORAGE_PROVIDER %q must be s3, minio or r2", c.Storage.Provider)
		}
		if c.Storage.UploadURLTTLSeconds <= 0 || c.Storage.DownloadURLTTLSeconds <= 0 {
			bad("AEDST_STORAGE_*_URL_TTL_SECONDS must be positive")
		}
		if c.Env.IsDeployed() && strings.HasPrefix(c.Storage.PublicEndpoint, "http://") {
			bad("AEDST_STORAGE_PUBLIC_ENDPOINT must use https in %s", c.Env)
		}
	}
	if c.Push.WebPushEnabled() && c.Push.VAPIDSubject == "" {
		bad("AEDST_PUSH_VAPID_SUBJECT (mailto: or https: contact) is required with AEDST_PUSH_VAPID_PRIVATE_KEY")
	}
	if c.Push.APNsEnabled() && c.Push.APNsTeamID == "" {
		bad("AEDST_PUSH_APNS_TEAM_ID is required with AEDST_PUSH_APNS_KEY_ID")
	}
	m := c.Media
	if m.MaxImageBytes < 0 || m.MaxVideoBytes < 0 || m.MaxAudioBytes < 0 || m.MaxDocumentBytes < 0 || m.MaxImagePixels < 0 || m.ProcessTimeoutSeconds < 0 || m.PendingUploadTTLHours < 0 {
		bad("AEDST_MEDIA_* limits must not be negative")
	}
	if c.Tasks.DueSoonLeadMinutes < 0 {
		bad("AEDST_TASKS_DUE_SOON_LEAD_MINUTES must not be negative")
	}
	if c.Retention.AuditDays < 0 {
		bad("AEDST_RETENTION_AUDIT_DAYS must not be negative")
	}
}
