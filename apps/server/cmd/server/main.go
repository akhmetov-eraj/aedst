// Command server is the aedst-server entrypoint. It only wires dependencies —
// config, platform, modules — together; it contains no business logic (see
// docs/architecture.md).
package main

import (
	"context"
	"log/slog"
	"os"
	"os/signal"
	"syscall"
	"time"

	"connectrpc.com/connect"
	"github.com/go-chi/chi/v5"

	authv1connect "github.com/aedst/server/gen/go/aedst/auth/v1/authv1connect"
	tasksv1connect "github.com/aedst/server/gen/go/aedst/tasks/v1/tasksv1connect"

	authapplication "github.com/aedst/server/internal/modules/auth/application"
	authpostgres "github.com/aedst/server/internal/modules/auth/infrastructure/postgres"
	authsecurity "github.com/aedst/server/internal/modules/auth/infrastructure/security"
	authtokens "github.com/aedst/server/internal/modules/auth/infrastructure/tokens"
	authtransport "github.com/aedst/server/internal/modules/auth/transport"

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
	platformjobs "github.com/aedst/server/internal/platform/jobs"
	"github.com/aedst/server/internal/platform/messaging"
	"github.com/aedst/server/internal/platform/observability"
	"github.com/aedst/server/internal/platform/outbox"
	platformredis "github.com/aedst/server/internal/platform/redis"
	sharedtime "github.com/aedst/server/internal/shared/time"

	"github.com/riverqueue/river"
)

func main() {
	if err := run(); err != nil {
		slog.Error("server exited with error", slog.Any("error", err))
		os.Exit(1)
	}
}

func run() error {
	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()

	cfg, err := config.Load()
	if err != nil {
		return err
	}

	logger := observability.NewLogger(cfg.Obs.LogLevel, cfg.Obs.LogFormat, cfg.Obs.ServiceName)
	slog.SetDefault(logger)

	tp, err := observability.NewTracerProvider(ctx, cfg.Obs.ServiceName, cfg.Obs.OTLPEndpoint)
	if err != nil {
		return err
	}
	defer func() { _ = tp.Shutdown(context.Background()) }()
	tracer := tp.Tracer(cfg.Obs.ServiceName)
	metrics := observability.NewMetrics()

	pool, err := database.NewPool(ctx, database.Config{
		DSN:      cfg.DB.DSN,
		MaxConns: cfg.DB.MaxConns,
		MinConns: cfg.DB.MinConns,
	})
	if err != nil {
		return err
	}
	defer pool.Close()

	redisClient, err := platformredis.NewClient(ctx, platformredis.Config{
		Addr:     cfg.Redis.Addr,
		Password: cfg.Redis.Password,
		DB:       cfg.Redis.DB,
	})
	if err != nil {
		return err
	}
	defer func() { _ = redisClient.Close() }()

	// NATS/JetStream stays platform infrastructure even though no module
	// currently publishes an integration event (auth's merge with the old
	// users module removed the only one, UserRegistered — see
	// docs/decisions/0007-merge-users-into-auth.md). A future module that
	// needs to react to another module's state changes without a
	// synchronous call reaches for this, not a new mechanism.
	natsClient, err := messaging.Connect(cfg.NATS.URL)
	if err != nil {
		return err
	}
	defer natsClient.Close()

	tokens, err := platformauth.NewTokenService(platformauth.Config{
		Issuer:          cfg.Obs.ServiceName,
		SigningKey:      cfg.Auth.JWTSigningKey,
		PreviousKey:     cfg.Auth.JWTPreviousSigningKey,
		AccessTokenTTL:  time.Duration(cfg.Auth.AccessTokenTTLMinutes) * time.Minute,
		RefreshTokenTTL: time.Duration(cfg.Auth.RefreshTokenTTLHours) * time.Hour,
	})
	if err != nil {
		return err
	}

	clock := sharedtime.Real{}

	// --- auth module (owns credentials, sessions, and identity — see
	// docs/decisions/0007-merge-users-into-auth.md) ---
	authService := authapplication.NewService(
		authpostgres.NewCredentialRepository(pool),
		authpostgres.NewSessionRepository(pool),
		authtokens.NewAdapter(tokens),
		authsecurity.NewBcryptHasher(),
		clock,
	)
	authHandler := authtransport.NewHandler(authService)

	// --- background jobs (River, see docs/decisions/0005-river-for-jobs.md) ---
	// Each module registers its own job kinds' workers here; this is the only
	// place job wiring happens, mirroring how modules are wired above.
	// jobsClient is constructed before tasksService below because the tasks
	// module's ReminderScheduler wraps it.
	jobWorkers := river.NewWorkers()

	if err := platformjobs.Migrate(ctx, pool); err != nil {
		return err
	}
	jobsClient, err := platformjobs.NewClient(pool, jobWorkers)
	if err != nil {
		return err
	}

	// --- tasks module (the reference module — see docs/modules.md) ---
	tasksOutboxRepo := taskspostgres.NewOutboxRepository(pool)
	tasksRepo := taskspostgres.NewRepository(pool, tasksOutboxRepo, taskspostgres.NewIdempotencyStore(), clock)
	tasksService := tasksapplication.NewService(
		tasksRepo,
		tasksjobsinfra.NewScheduler(jobsClient),
		clock,
	)
	tasksHandler := taskstransport.NewHandler(tasksService)
	river.AddWorker(jobWorkers, tasksjobs.NewDueReminderWorker(tasksRepo, logger))

	// The outbox Forwarder publishes tasks.v1.TaskCompleted rows (written by
	// Repository.Complete in the same transaction as the status change) to
	// NATS. See internal/platform/outbox — this is the one place a module's
	// Forwarder is started; a future module with its own outbox rows adds
	// its own Forwarder here the same way.
	tasksOutboxForwarder := outbox.NewForwarder("tasks", tasksOutboxRepo, natsClient, outbox.JSONEnvelope, logger, clock, outbox.DefaultForwarderConfig())
	forwarderCtx, stopForwarders := context.WithCancel(ctx)
	defer stopForwarders()
	go func() {
		if err := tasksOutboxForwarder.Run(forwarderCtx); err != nil {
			logger.ErrorContext(forwarderCtx, "tasks outbox forwarder stopped", slog.Any("error", err))
		}
	}()

	// Procedures reachable without a token: registration/login/refresh happen
	// before a client has one (see platform/httpserver.AuthInterceptor).
	publicProcedures := map[string]bool{
		authv1connect.AuthServiceRegisterProcedure:     true,
		authv1connect.AuthServiceLoginProcedure:        true,
		authv1connect.AuthServiceRefreshTokenProcedure: true,
		authv1connect.AuthServiceLogoutProcedure:       true,
	}
	interceptors := connect.WithInterceptors(
		httpserver.AuthInterceptor(tokens, publicProcedures),
		idempotency.HeaderInterceptor(),
	)

	router := httpserver.NewRouter(httpserver.Options{
		Logger:         logger,
		Tracer:         tracer,
		Metrics:        metrics,
		Tokens:         tokens,
		RequestTimeout: 30 * time.Second,
		AllowedOrigins: cfg.HTTP.AllowedOrigins,
	})

	authPath, authConnectHandler := authv1connect.NewAuthServiceHandler(authHandler, interceptors)
	router.Mount(authPath, authConnectHandler)

	tasksPath, tasksConnectHandler := tasksv1connect.NewTasksServiceHandler(tasksHandler, interceptors)
	router.Mount(tasksPath, tasksConnectHandler)

	metricsRouter := chi.NewRouter()
	metricsRouter.Handle("/metrics", observability.Handler())
	metricsServer := httpserver.New(cfg.HTTP.MetricsAddr, metricsRouter)
	go func() {
		if err := metricsServer.ListenAndServe(); err != nil {
			logger.ErrorContext(ctx, "metrics server stopped", slog.Any("error", err))
		}
	}()

	server := httpserver.New(cfg.HTTP.Addr, router)
	go func() {
		logger.Info("aedst-server listening", slog.String("addr", cfg.HTTP.Addr))
		if err := server.ListenAndServe(); err != nil {
			logger.ErrorContext(ctx, "http server stopped", slog.Any("error", err))
		}
	}()

	if err := platformjobs.Start(ctx, jobsClient); err != nil {
		return err
	}

	<-ctx.Done()
	logger.Info("shutting down")

	shutdownCtx, cancel := context.WithTimeout(context.Background(), time.Duration(cfg.HTTP.ShutdownTimeout)*time.Second)
	defer cancel()

	_ = server.Shutdown(shutdownCtx)
	_ = metricsServer.Shutdown(shutdownCtx)
	_ = platformjobs.Stop(shutdownCtx, jobsClient)

	return nil
}
