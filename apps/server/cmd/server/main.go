// Command server is the aedst-server entrypoint. It only wires dependencies —
// config, platform, modules — together; it contains no business logic (see
// docs/content/architecture.md).
package main

import (
	"context"
	"errors"
	"log/slog"
	"maps"
	"os"
	"os/signal"
	"sync"
	"syscall"
	"time"

	"connectrpc.com/connect"
	"connectrpc.com/grpchealth"
	"connectrpc.com/grpcreflect"
	"connectrpc.com/otelconnect"
	"github.com/go-chi/chi/v5"
	"github.com/riverqueue/river"
	"golang.org/x/sync/errgroup"

	adminv1 "github.com/aedst/server/gen/go/aedst/admin/v1"
	adminv1connect "github.com/aedst/server/gen/go/aedst/admin/v1/adminv1connect"
	authv1 "github.com/aedst/server/gen/go/aedst/auth/v1"
	authv1connect "github.com/aedst/server/gen/go/aedst/auth/v1/authv1connect"
	mediav1 "github.com/aedst/server/gen/go/aedst/media/v1"
	mediav1connect "github.com/aedst/server/gen/go/aedst/media/v1/mediav1connect"
	notificationsv1 "github.com/aedst/server/gen/go/aedst/notifications/v1"
	notificationsv1connect "github.com/aedst/server/gen/go/aedst/notifications/v1/notificationsv1connect"
	searchv1 "github.com/aedst/server/gen/go/aedst/search/v1"
	searchv1connect "github.com/aedst/server/gen/go/aedst/search/v1/searchv1connect"
	tasksv1 "github.com/aedst/server/gen/go/aedst/tasks/v1"
	tasksv1connect "github.com/aedst/server/gen/go/aedst/tasks/v1/tasksv1connect"

	adminapplication "github.com/aedst/server/internal/modules/admin/application"
	adminpostgres "github.com/aedst/server/internal/modules/admin/infrastructure/postgres"
	admintransport "github.com/aedst/server/internal/modules/admin/transport"

	authapi "github.com/aedst/server/internal/modules/auth/api"
	authapplication "github.com/aedst/server/internal/modules/auth/application"
	authpostgres "github.com/aedst/server/internal/modules/auth/infrastructure/postgres"
	authratelimit "github.com/aedst/server/internal/modules/auth/infrastructure/ratelimit"
	authsecurity "github.com/aedst/server/internal/modules/auth/infrastructure/security"
	authtokens "github.com/aedst/server/internal/modules/auth/infrastructure/tokens"
	authtransport "github.com/aedst/server/internal/modules/auth/transport"

	mediaapplication "github.com/aedst/server/internal/modules/media/application"
	mediadomain "github.com/aedst/server/internal/modules/media/domain"
	mediapostgres "github.com/aedst/server/internal/modules/media/infrastructure/postgres"
	mediaprocessing "github.com/aedst/server/internal/modules/media/infrastructure/processing"
	mediatasks "github.com/aedst/server/internal/modules/media/infrastructure/tasks"
	mediatransport "github.com/aedst/server/internal/modules/media/transport"

	notificationsapplication "github.com/aedst/server/internal/modules/notifications/application"
	notificationsevents "github.com/aedst/server/internal/modules/notifications/infrastructure/events"
	notificationsjobs "github.com/aedst/server/internal/modules/notifications/infrastructure/jobs"
	notificationspostgres "github.com/aedst/server/internal/modules/notifications/infrastructure/postgres"
	notificationstransport "github.com/aedst/server/internal/modules/notifications/transport"

	searchapplication "github.com/aedst/server/internal/modules/search/application"
	searchevents "github.com/aedst/server/internal/modules/search/infrastructure/events"
	searchpostgres "github.com/aedst/server/internal/modules/search/infrastructure/postgres"
	searchtransport "github.com/aedst/server/internal/modules/search/transport"

	tasksapplication "github.com/aedst/server/internal/modules/tasks/application"
	tasksjobsinfra "github.com/aedst/server/internal/modules/tasks/infrastructure/jobs"
	taskspostgres "github.com/aedst/server/internal/modules/tasks/infrastructure/postgres"
	tasksjobs "github.com/aedst/server/internal/modules/tasks/jobs"
	taskstransport "github.com/aedst/server/internal/modules/tasks/transport"

	platformauth "github.com/aedst/server/internal/platform/auth"
	"github.com/aedst/server/internal/platform/config"
	"github.com/aedst/server/internal/platform/database"
	"github.com/aedst/server/internal/platform/httpserver"
	"github.com/aedst/server/internal/platform/idempotency"
	"github.com/aedst/server/internal/platform/inbox"
	platformjobs "github.com/aedst/server/internal/platform/jobs"
	"github.com/aedst/server/internal/platform/messaging"
	"github.com/aedst/server/internal/platform/observability"
	"github.com/aedst/server/internal/platform/outbox"
	"github.com/aedst/server/internal/platform/push"
	"github.com/aedst/server/internal/platform/retention"
	"github.com/aedst/server/internal/platform/storage"
	"github.com/aedst/server/internal/platform/valkey"
	sharedtime "github.com/aedst/server/internal/shared/time"
)

// defaultRolePolicy closes operator services by default: an empty
// AEDST_ROLE_POLICY must not open AdminService to every signed-in user. An
// entry for the same key in AEDST_ROLE_POLICY replaces it.
var defaultRolePolicy = map[string][]string{
	"/" + adminv1connect.AdminServiceName + "/*": {"admin"},
}

// Per-service request message caps (bytes). Media/search/admin requests are
// small metadata; bytes of uploads never pass through the API.
const smallRequestBytes = 64 << 10

func main() {
	os.Exit(exitCode())
}

// exitCode runs the server and maps its result to a process exit status; it is a
// separate function so the signal context is released before os.Exit.
func exitCode() int {
	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()

	if err := run(ctx); err != nil {
		slog.Error("server exited with error", slog.Any("error", err))
		return 1
	}
	return 0
}

func run(ctx context.Context) error {
	cfg, err := config.Load()
	if err != nil {
		return err
	}

	tel, err := observability.Setup(ctx, observability.Config{
		ServiceName:   cfg.Obs.ServiceName,
		LogLevel:      cfg.Obs.LogLevel,
		LogFormat:     cfg.Obs.LogFormat,
		ExportEnabled: cfg.Obs.ExportEnabled(),
	})
	if err != nil {
		return err
	}
	logger := tel.Logger
	slog.SetDefault(logger)
	// Flush telemetry last, after every component that emits it has stopped.
	defer func() {
		flushCtx, cancel := context.WithTimeout(context.WithoutCancel(ctx), 5*time.Second)
		defer cancel()
		if err := tel.Shutdown(flushCtx); err != nil {
			logger.Warn("telemetry shutdown", slog.Any("error", err))
		}
	}()

	pool, err := database.NewPool(ctx, database.Config{
		DSN:              cfg.DB.DSN,
		MaxConns:         cfg.DB.MaxConns,
		MinConns:         cfg.DB.MinConns,
		MaxConnLifetime:  time.Duration(cfg.DB.MaxConnLifetimeMinutes) * time.Minute,
		MaxConnIdleTime:  time.Duration(cfg.DB.MaxConnIdleMinutes) * time.Minute,
		StatementTimeout: time.Duration(cfg.DB.StatementTimeoutMs) * time.Millisecond,
		IdleInTxTimeout:  time.Duration(cfg.DB.IdleInTxTimeoutMs) * time.Millisecond,
		ApplicationName:  cfg.Obs.ServiceName,
	})
	if err != nil {
		return err
	}
	defer pool.Close()

	// Background work (River, the outbox relay, event consumers) gets its own
	// pool so a burst of jobs can only exhaust its own connections, never the
	// ones request handlers need — and vice versa. It also lets that pool
	// bypass a transaction-pooling proxy (PgBouncer, RDS Proxy) via
	// AEDST_DATABASE_DIRECT_DSN: River's LISTEN/NOTIFY and platform/lock's
	// session-level advisory locks do not survive transaction pooling. A
	// job-enqueue inside a request transaction (InsertTx) still rides that
	// transaction on the main pool.
	jobsDSN := cfg.DB.DSN
	if cfg.DB.DirectDSN != "" {
		jobsDSN = cfg.DB.DirectDSN
	}
	jobsPool, err := database.NewPool(ctx, database.Config{
		DSN:              jobsDSN,
		MaxConns:         cfg.DB.JobsMaxConns,
		MinConns:         1,
		MaxConnLifetime:  time.Duration(cfg.DB.MaxConnLifetimeMinutes) * time.Minute,
		MaxConnIdleTime:  time.Duration(cfg.DB.MaxConnIdleMinutes) * time.Minute,
		StatementTimeout: time.Duration(cfg.DB.StatementTimeoutMs) * time.Millisecond,
		IdleInTxTimeout:  time.Duration(cfg.DB.IdleInTxTimeoutMs) * time.Millisecond,
		ApplicationName:  cfg.Obs.ServiceName + "-jobs",
	})
	if err != nil {
		return err
	}
	defer jobsPool.Close()

	// Pool saturation is the first thing that breaks under load; export it.
	if err := database.RegisterPoolMetrics("api", pool); err != nil {
		return err
	}
	if err := database.RegisterPoolMetrics("jobs", jobsPool); err != nil {
		return err
	}

	// Valkey (cache, rate limiting) is optional: nothing correct depends on it,
	// so it neither blocks startup nor readiness. The limiters fail open.
	valkeyClient := valkey.NewClient(valkey.Config{
		Addr:     cfg.Valkey.Addr,
		Password: cfg.Valkey.Password,
		DB:       cfg.Valkey.DB,
	})
	defer func() { _ = valkeyClient.Close() }()
	if err := valkey.Ping(ctx, valkeyClient); err != nil {
		logger.Warn("valkey unreachable at startup; rate limiting will fail open until it is back", slog.Any("error", err))
	}
	rateLimiter := valkey.NewRateLimiter(valkeyClient)

	// --- authentication back-ends: first-party tokens and/or an external
	// OIDC provider, verified through one platformauth.Verifier. ---
	var firstParty *platformauth.TokenService
	if cfg.Auth.FirstPartyEnabled {
		firstParty, err = platformauth.NewTokenService(platformauth.Config{
			Issuer:          cfg.Obs.ServiceName,
			SigningKey:      cfg.Auth.JWTSigningKey,
			PreviousKey:     cfg.Auth.JWTPreviousSigningKey,
			AccessTokenTTL:  time.Duration(cfg.Auth.AccessTokenTTLMinutes) * time.Minute,
			RefreshTokenTTL: time.Duration(cfg.Auth.RefreshTokenTTLHours) * time.Hour,
		})
		if err != nil {
			return err
		}
	}
	var oidcVerifier *platformauth.OIDCVerifier
	if cfg.OIDC.Issuer != "" {
		oidcVerifier, err = platformauth.NewOIDCVerifier(platformauth.OIDCConfig{
			Issuer:     cfg.OIDC.Issuer,
			Audience:   cfg.OIDC.Audience,
			JWKSURL:    cfg.OIDC.JWKSURL,
			Algorithms: cfg.OIDC.Algorithms,
			RolesClaim: cfg.OIDC.RolesClaim,
			ClockSkew:  time.Duration(cfg.OIDC.ClockSkewSeconds) * time.Second,
		})
		if err != nil {
			return err
		}
		defer func() { _ = oidcVerifier.Close(context.WithoutCancel(ctx)) }()
	}
	verifier, err := platformauth.NewMultiVerifier(firstParty, oidcVerifier)
	if err != nil {
		return err
	}

	clock := sharedtime.Real{}

	// --- object storage (media). Optional: without a bucket MediaService
	// answers FAILED_PRECONDITION and the server still starts. ---
	var objects storage.ObjectStorage = storage.Disabled{}
	storageEnabled := cfg.Storage.Bucket != ""
	if storageEnabled {
		s3, err := storage.NewS3(ctx, storage.Config{
			Provider:        cfg.Storage.Provider,
			Endpoint:        cfg.Storage.Endpoint,
			PublicEndpoint:  cfg.Storage.PublicEndpoint,
			Region:          cfg.Storage.Region,
			Bucket:          cfg.Storage.Bucket,
			AccessKeyID:     cfg.Storage.AccessKeyID,
			SecretAccessKey: cfg.Storage.SecretAccessKey,
			PathStyle:       cfg.Storage.PathStyle,
		})
		if err != nil {
			return err
		}
		objects = s3
	}

	// --- push providers: each disabled while unconfigured ---
	pushClient := push.NewHTTPClient(time.Duration(cfg.Push.TimeoutSeconds) * time.Second)
	dispatcher := &push.Dispatcher{}
	webPushKey := ""
	if cfg.Push.WebPushEnabled() {
		wp, err := push.NewWebPush(push.WebPushConfig{PublicKey: cfg.Push.VAPIDPublicKey, PrivateKey: cfg.Push.VAPIDPrivateKey, Subject: cfg.Push.VAPIDSubject}, pushClient)
		if err != nil {
			return err
		}
		dispatcher.WebPush, webPushKey = wp, wp.PublicKey()
	}
	if cfg.Push.FCMEnabled() {
		fcm, err := push.NewFCM(ctx, push.FCMConfig{CredentialsJSON: cfg.Push.FCMCredentialsJSON, CredentialsFile: cfg.Push.FCMCredentialsFile, ProjectID: cfg.Push.FCMProjectID}, pushClient)
		if err != nil {
			return err
		}
		dispatcher.FCM = fcm
	}
	if cfg.Push.APNsEnabled() {
		apns, err := push.NewAPNs(push.APNsConfig{KeyID: cfg.Push.APNsKeyID, TeamID: cfg.Push.APNsTeamID, PrivateKey: cfg.Push.APNsPrivateKey, PrivateKeyFile: cfg.Push.APNsPrivateKeyFile, DefaultTopic: cfg.Push.APNsTopic}, pushClient)
		if err != nil {
			return err
		}
		dispatcher.APNs = apns
	}
	logger.Info("push providers", slog.Any("enabled", dispatcher.Enabled()))

	// --- background jobs (River, see docs/content/decisions/0005-river-for-jobs.md) ---
	// River runs deferred jobs (reminders, push delivery, media processing,
	// cleanup). Integration events do NOT go through it: they use the
	// transactional outbox and a batched Relay (see platform/outbox). Each
	// module registers its workers here; this is the only place job wiring
	// happens. AEDST_JOBS_QUEUES decides which queues THIS process works, so a
	// dedicated worker deployment can run only "media" (heavy processing)
	// while API replicas run "default".
	jobWorkers := river.NewWorkers()
	if err := platformjobs.Migrate(ctx, jobsPool); err != nil {
		return err
	}

	// Repositories on the jobs pool that retention and workers use. The media
	// one needs the River client (it enqueues object cleanup), which is built
	// below from the retention jobs — bound late through this variable.
	notificationsJobsRepo := notificationspostgres.NewRepository(jobsPool, nil)
	searchJobsRepo := searchpostgres.NewRepository(jobsPool)
	adminRepo := adminpostgres.NewRepository(pool, clock)
	var mediaJobsRepo *mediapostgres.Repository

	// Retention keeps the append-only tables bounded (one leader-elected River
	// periodic job per policy). The outbox purges itself in its relay.
	retentionPolicies := []retention.Policy{
		{
			Name:      "platform.inbox",
			Retention: time.Duration(cfg.Retention.InboxHours) * time.Hour,
			Purge: func(ctx context.Context, cutoff time.Time, limit int) (int64, error) {
				return inbox.Purge(ctx, jobsPool, cutoff, limit)
			},
		},
		{
			Name:      "platform.idempotency_keys",
			Retention: time.Duration(cfg.Retention.IdempotencyHours) * time.Hour,
			Purge: func(ctx context.Context, cutoff time.Time, limit int) (int64, error) {
				return idempotency.Purge(ctx, jobsPool, cutoff, limit)
			},
		},
		{
			Name:      "notifications.notifications",
			Retention: time.Duration(cfg.Retention.NotificationDays) * 24 * time.Hour,
			Purge:     notificationsJobsRepo.PurgeOlderThan,
		},
		{
			Name:      "notifications.push_devices",
			Retention: 30 * 24 * time.Hour, // disabled devices
			Purge:     notificationsJobsRepo.PurgeDisabledDevices,
		},
		{
			// Tombstones must outlive the outbox redelivery window, or a late
			// old snapshot could resurrect a deleted document.
			Name:      "search.tombstones",
			Retention: time.Duration(cfg.Retention.InboxHours) * time.Hour,
			Purge:     searchJobsRepo.PurgeTombstones,
		},
		{
			Name:      "admin.audit_log",
			Retention: time.Duration(cfg.Retention.AuditDays) * 24 * time.Hour,
			Purge:     adminRepo.Purge,
		},
		{
			Name:      "media.pending_uploads",
			Retention: time.Duration(cfg.Media.PendingUploadTTLHours) * time.Hour,
			Purge: func(ctx context.Context, cutoff time.Time, limit int) (int64, error) {
				return mediaJobsRepo.PurgeAbandoned(ctx, cutoff, limit)
			},
		},
	}
	if cfg.Auth.FirstPartyEnabled {
		retentionPolicies = append(retentionPolicies, retention.Policy{
			Name:      "auth.sessions",
			Retention: time.Duration(cfg.Retention.SessionHours) * time.Hour,
			Purge:     authpostgres.NewSessionRepository(jobsPool).PurgeDeadSessions,
		})
	}
	retentionJobs, err := retention.Register(jobWorkers, retentionPolicies, time.Duration(cfg.Retention.IntervalMinutes)*time.Minute, logger)
	if err != nil {
		return err
	}
	jobsClient, err := platformjobs.NewClientWithQueues(jobsPool, jobWorkers, logger, cfg.Jobs.Queues, retentionJobs...)
	if err != nil {
		return err
	}

	// --- integration events: outbox -> relay -> in-process bus -> inbox-guarded
	// subscribers. A module that reacts to events subscribes below, wrapping its
	// handler in an inbox.Consumer for exactly-once effect. ---
	bus := messaging.NewBus()
	var relays []*outbox.Relay
	newOutbox := func(module string) (*outbox.Writer, error) {
		repo := outbox.NewPostgresRepository(jobsPool, module)
		relay, err := outbox.NewRelay(repo, bus, outbox.RelayConfig{
			Module:      module,
			Interval:    time.Duration(cfg.Outbox.RelayIntervalMs) * time.Millisecond,
			BatchSize:   cfg.Outbox.BatchSize,
			Concurrency: cfg.Outbox.Concurrency,
			Retention:   time.Duration(cfg.Outbox.RetentionHours) * time.Hour,
		}, logger)
		if err != nil {
			return nil, err
		}
		relays = append(relays, relay)
		return outbox.NewWriter(repo), nil
	}

	// --- tasks module (the reference module — see docs/content/modules.md) ---
	tasksOutbox, err := newOutbox("tasks")
	if err != nil {
		return err
	}
	// The reminder job is enqueued inside the task's own transaction
	// (InsertTx on the request pool's tx), so it exists iff the change committed.
	tasksRepo := taskspostgres.NewRepository(pool, tasksOutbox, clock,
		taskspostgres.WithReminders(tasksjobsinfra.NewScheduler(jobsClient, time.Duration(cfg.Tasks.DueSoonLeadMinutes)*time.Minute)))
	tasksService := tasksapplication.NewService(tasksRepo, clock)
	tasksAdmin := tasksapplication.NewAdminService(tasksRepo)
	// The reminder worker runs on the jobs pool and publishes TaskDueSoon
	// through the same outbox.
	river.AddWorker(jobWorkers, tasksjobs.NewDueReminderWorker(
		taskspostgres.NewRepository(jobsPool, tasksOutbox, clock), logger))

	// --- notifications module: consumes tasks/media events, serves the inbox,
	// the push-device registry and preferences, delivers push from River. ---
	notificationsRepo := notificationspostgres.NewRepository(pool, jobsClient)
	notificationsConsumerRepo := notificationspostgres.NewRepository(jobsPool, jobsClient)
	notificationsevents.Subscribe(bus, jobsPool, notificationsConsumerRepo, logger)
	river.AddWorker(jobWorkers, notificationsjobs.NewFanOutWorker(notificationsConsumerRepo, logger))
	river.AddWorker(jobWorkers, notificationsjobs.NewDeviceWorker(notificationsConsumerRepo, dispatcher, logger))
	notificationsService := notificationsapplication.NewService(notificationsRepo, clock, notificationsapplication.WithWebPushPublicKey(webPushKey))

	// --- search module: a projection of tasks/media events (schema search) ---
	searchevents.Subscribe(bus, jobsPool, searchJobsRepo, logger)
	searchService := searchapplication.NewService(searchpostgres.NewRepository(pool))

	// --- media module: presigned direct uploads; processing on the "media" queue ---
	mediaOutbox, err := newOutbox("media")
	if err != nil {
		return err
	}
	mediaRepo := mediapostgres.NewRepository(pool, jobsClient, mediaOutbox, clock)
	mediaJobsRepo = mediapostgres.NewRepository(jobsPool, jobsClient, mediaOutbox, clock)
	river.AddWorker(jobWorkers, mediaprocessing.NewWorker(mediaJobsRepo, objects, mediaprocessing.Config{
		MaxImagePixels: cfg.Media.MaxImagePixels,
		FFmpeg: mediaprocessing.FFmpeg{
			FFmpegPath:  cfg.Media.FFmpegPath,
			FFprobePath: cfg.Media.FFprobePath,
			Timeout:     time.Duration(cfg.Media.ProcessTimeoutSeconds) * time.Second,
		},
	}, logger))
	river.AddWorker(jobWorkers, mediaprocessing.NewDeleteWorker(objects))
	mediaService := mediaapplication.NewService(mediaRepo, objects, mediatasks.Ownership{Tasks: tasksService}, clock, mediaapplication.Config{
		Limits: mediadomain.Limits{
			MaxImageBytes: cfg.Media.MaxImageBytes, MaxVideoBytes: cfg.Media.MaxVideoBytes,
			MaxAudioBytes: cfg.Media.MaxAudioBytes, MaxDocumentBytes: cfg.Media.MaxDocumentBytes,
		},
		UploadURLTTL:   time.Duration(cfg.Storage.UploadURLTTLSeconds) * time.Second,
		DownloadURLTTL: time.Duration(cfg.Storage.DownloadURLTTLSeconds) * time.Second,
	}, storageEnabled)

	// --- auth (first-party, optional) and the operator directory it backs ---
	var directory authapi.Directory
	if cfg.Auth.FirstPartyEnabled {
		directory = authapplication.NewDirectory(authpostgres.NewCredentialRepository(pool))
	}

	// --- admin module: composes the modules above through their api/ packages ---
	adminService := adminapplication.NewService(adminapplication.Deps{
		Tasks:         tasksAdmin,
		Notifications: notificationsService,
		Directory:     directory,
		Media:         mediaService,
		Platform:      adminRepo,
		Audit:         adminRepo,
		Tx:            adminRepo,
		Clock:         clock,
	})

	// Procedures reachable without a token: registration/login/refresh happen
	// before a client has one (see platform/httpserver.AuthInterceptor).
	publicProcedures := map[string]bool{
		authv1connect.AuthServiceRegisterProcedure:     true,
		authv1connect.AuthServiceLoginProcedure:        true,
		authv1connect.AuthServiceRefreshTokenProcedure: true,
		authv1connect.AuthServiceLogoutProcedure:       true,
	}

	otelInterceptor, err := otelconnect.NewInterceptor()
	if err != nil {
		return err
	}
	// rolePolicy maps a procedure (or "/pkg.Service/*") to the roles of which
	// the caller needs one: built-in defaults (AdminService=admin) overlaid by
	// AEDST_ROLE_POLICY. Validated against the real procedures, so a typo fails
	// the deploy instead of silently leaving an RPC open.
	configuredPolicy, err := httpserver.ParseRolePolicy(cfg.Auth.RolePolicy)
	if err != nil {
		return err
	}
	rolePolicy := httpserver.MergeRolePolicy(defaultRolePolicy, configuredPolicy)
	knownProcedures := httpserver.ProceduresOf(
		authv1.File_aedst_auth_v1_auth_proto,
		tasksv1.File_aedst_tasks_v1_tasks_proto,
		notificationsv1.File_aedst_notifications_v1_notifications_proto,
		searchv1.File_aedst_search_v1_search_proto,
		mediav1.File_aedst_media_v1_media_proto,
		adminv1.File_aedst_admin_v1_admin_proto,
	)
	if err := httpserver.ValidateRolePolicy(rolePolicy, knownProcedures, publicProcedures); err != nil {
		return err
	}

	// Order matters: trace everything, log server-side failures with their real
	// cause, turn away unsupported client versions (the cheapest check), throttle
	// unauthenticated auth RPCs before spending any CPU on them, authenticate,
	// authorize by role, throttle the authenticated caller per user, then
	// validate the request against its protovalidate constraints (added by the
	// mounter, always), then idempotency.
	before := []connect.Interceptor{otelInterceptor, httpserver.ErrorLogInterceptor(logger), httpserver.ClientVersionInterceptor(cfg.Client.MinVersions, logger)}
	if cfg.HTTP.AuthRateLimitPerMinute > 0 {
		before = append(before, httpserver.RateLimitInterceptor(rateLimiter, publicProcedures, cfg.HTTP.AuthRateLimitPerMinute, logger))
	}
	before = append(before,
		httpserver.AuthInterceptor(verifier, publicProcedures, logger),
		httpserver.RoleInterceptor(rolePolicy),
	)
	if cfg.HTTP.APIRateLimitPerMinute > 0 {
		before = append(before, httpserver.UserRateLimitInterceptor(rateLimiter, cfg.HTTP.APIRateLimitPerMinute, logger))
	}
	after := []connect.Interceptor{idempotency.HeaderInterceptor()}

	readiness := httpserver.NewReadiness(
		httpserver.Check{Name: "database", Fn: pool.Ping},
		// Optional: a Valkey outage degrades rate limiting (which fails open); it
		// must not pull every replica out of rotation.
		httpserver.Check{Name: "valkey", Optional: true, Fn: func(ctx context.Context) error { return valkey.Ping(ctx, valkeyClient) }},
	)
	streaming := map[string]bool{} // filled once every service is mounted, before serving
	router := httpserver.NewRouter(httpserver.Options{
		Logger:              logger,
		RequestTimeout:      time.Duration(cfg.HTTP.RequestTimeout) * time.Second,
		AllowedOrigins:      cfg.HTTP.AllowedOrigins,
		TrustedProxyHops:    cfg.HTTP.TrustedProxyHops,
		Readiness:           readiness,
		MaxInFlight:         cfg.HTTP.MaxInFlight,
		MaxBodyBytes:        int64(cfg.HTTP.MaxRequestBytes) + 4096, // message cap + envelope/framing
		HSTSMaxAge:          time.Duration(cfg.Server.HSTSMaxAgeSeconds) * time.Second,
		StreamingProcedures: streaming,
	})

	rpc := newRPCMounter(router, before, after, cfg.HTTP.MaxRequestBytes, cfg.Server.RequireConnectProtocolHeader)

	// The first-party AuthService is mounted only when enabled; with an
	// external IdP as the system of record, clients authenticate against it
	// directly and this server only verifies its tokens.
	if cfg.Auth.FirstPartyEnabled {
		var authOpts []authapplication.Option
		authOpts = append(authOpts, authapplication.WithLogger(logger))
		if cfg.Auth.LoginRateLimitPerMinute > 0 {
			authOpts = append(authOpts, authapplication.WithLoginLimiter(authratelimit.NewLoginLimiter(rateLimiter, cfg.Auth.LoginRateLimitPerMinute)))
		}
		hasher, err := authsecurity.NewArgon2idHasher()
		if err != nil {
			return err
		}
		authService := authapplication.NewService(
			authpostgres.NewCredentialRepository(pool),
			authpostgres.NewSessionRepository(pool),
			authtokens.NewAdapter(firstParty),
			hasher,
			clock,
			authOpts...,
		)
		mountService(rpc, authv1connect.NewAuthServiceHandler, authv1connect.AuthServiceHandler(authtransport.NewHandler(authService)), smallRequestBytes)
	}
	mountService(rpc, tasksv1connect.NewTasksServiceHandler, tasksv1connect.TasksServiceHandler(taskstransport.NewHandler(tasksService)), 0)
	mountService(rpc, notificationsv1connect.NewNotificationsServiceHandler, notificationsv1connect.NotificationsServiceHandler(notificationstransport.NewHandler(notificationsService)), smallRequestBytes)
	mountService(rpc, searchv1connect.NewSearchServiceHandler, searchv1connect.SearchServiceHandler(searchtransport.NewHandler(searchService)), smallRequestBytes)
	mountService(rpc, mediav1connect.NewMediaServiceHandler, mediav1connect.MediaServiceHandler(mediatransport.NewHandler(mediaService)), smallRequestBytes)
	mountService(rpc, adminv1connect.NewAdminServiceHandler, adminv1connect.AdminServiceHandler(admintransport.NewHandler(adminService)), smallRequestBytes)
	if err := rpc.verifyKnown(knownProcedures); err != nil {
		return err
	}
	maps.Copy(streaming, rpc.streamingProcedures())

	// Standard gRPC health (driven by the same Readiness as /readyz) and, when
	// enabled, server reflection. Neither sits behind auth: probes must work
	// without a token, and reflection is opt-in for that reason.
	services := rpc.services()
	router.Mount(grpchealth.NewHandler(httpserver.NewHealthChecker(readiness, services...)))
	if cfg.HTTP.GRPCReflectionEnabled {
		reflector := grpcreflect.NewStaticReflector(services...)
		router.Mount(grpcreflect.NewHandlerV1(reflector))
		router.Mount(grpcreflect.NewHandlerV1Alpha(reflector))
	}

	server := httpserver.NewWithConfig(httpserver.ServerConfig{
		Addr:              cfg.HTTP.Addr,
		ReadHeaderTimeout: time.Duration(cfg.Server.ReadHeaderTimeoutSeconds) * time.Second,
		ReadTimeout:       time.Duration(cfg.Server.ReadTimeoutSeconds) * time.Second,
		WriteTimeout:      time.Duration(cfg.Server.WriteTimeoutSeconds) * time.Second,
		IdleTimeout:       time.Duration(cfg.Server.IdleTimeoutSeconds) * time.Second,
		MaxHeaderBytes:    cfg.Server.MaxHeaderBytes,
		TLSCertFile:       cfg.Server.TLSCertFile,
		TLSKeyFile:        cfg.Server.TLSKeyFile,
	}, httpserver.Instrument(router, cfg.Obs.ServiceName), logger)

	// Optional Prometheus scrape endpoint on its own listener, so it is never
	// exposed on the public port. OTLP push works with or without it.
	var metricsServer *httpserver.Server
	if cfg.HTTP.MetricsAddr != "" {
		metricsRouter := chi.NewRouter()
		metricsRouter.Handle("/metrics", tel.MetricsHandler)
		metricsServer = httpserver.New(cfg.HTTP.MetricsAddr, metricsRouter, logger)
	}

	// River workers outlive the signal context: they are stopped gracefully
	// below (letting in-flight jobs finish) rather than hard-cancelled. The
	// outbox relays follow the same rule and are stopped after the HTTP
	// server has drained, so events committed by in-flight requests are still
	// delivered. An insert-only process (AEDST_JOBS_QUEUES empty) never starts
	// River's fetch loops.
	jobsWork := platformjobs.Works(cfg.Jobs.Queues)
	if jobsWork {
		if err := platformjobs.Start(context.WithoutCancel(ctx), jobsClient); err != nil {
			return err
		}
	}
	relayCtx, stopRelays := context.WithCancel(context.WithoutCancel(ctx))
	defer stopRelays()
	var relaysDone sync.WaitGroup
	for _, relay := range relays {
		relaysDone.Go(func() { _ = relay.Run(relayCtx) })
	}

	group, groupCtx := errgroup.WithContext(ctx)
	group.Go(func() error {
		logger.Info("aedst-server listening", slog.String("addr", cfg.HTTP.Addr), slog.String("env", string(cfg.Env)),
			slog.Any("job_queues", cfg.Jobs.Queues), slog.Bool("tls", cfg.Server.TLSCertFile != ""))
		return server.ListenAndServe()
	})
	if metricsServer != nil {
		group.Go(func() error {
			logger.Info("metrics listening", slog.String("addr", cfg.HTTP.MetricsAddr))
			return metricsServer.ListenAndServe()
		})
	}

	// Graceful shutdown: fail readiness first so the load balancer stops
	// routing, optionally wait for it to notice, stop accepting and drain
	// in-flight requests, then stop the outbox relays and let River finish
	// running jobs.
	group.Go(func() error {
		<-groupCtx.Done()
		logger.Info("shutting down")
		readiness.SetDraining()
		if drain := time.Duration(cfg.HTTP.ShutdownDrainSeconds) * time.Second; drain > 0 {
			time.Sleep(drain)
		}

		shutdownCtx, cancel := context.WithTimeout(context.WithoutCancel(groupCtx), time.Duration(cfg.HTTP.ShutdownTimeout)*time.Second)
		defer cancel()

		var errs []error
		errs = append(errs, server.Shutdown(shutdownCtx))
		if metricsServer != nil {
			errs = append(errs, metricsServer.Shutdown(shutdownCtx))
		}
		stopRelays()
		relaysDone.Wait()
		if jobsWork {
			errs = append(errs, platformjobs.Stop(shutdownCtx, jobsClient))
		}
		return errors.Join(errs...)
	})

	return group.Wait()
}
