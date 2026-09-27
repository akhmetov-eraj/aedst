// Command server is the aedst-server entrypoint. It only wires dependencies —
// config, platform, modules — together; it contains no business logic (see
// docs/architecture.md).
package main

import (
	"context"
	"errors"
	"log/slog"
	"os"
	"os/signal"
	"syscall"
	"time"

	"connectrpc.com/connect"
	"github.com/go-chi/chi/v5"

	authv1connect "github.com/aedst/server/gen/go/aedst/auth/v1/authv1connect"
	profilev1connect "github.com/aedst/server/gen/go/aedst/profile/v1/profilev1connect"
	usersv1connect "github.com/aedst/server/gen/go/aedst/users/v1/usersv1connect"

	authapplication "github.com/aedst/server/internal/modules/auth/application"
	authpostgres "github.com/aedst/server/internal/modules/auth/infrastructure/postgres"
	authsecurity "github.com/aedst/server/internal/modules/auth/infrastructure/security"
	authtokens "github.com/aedst/server/internal/modules/auth/infrastructure/tokens"
	authtransport "github.com/aedst/server/internal/modules/auth/transport"

	profileapplication "github.com/aedst/server/internal/modules/profile/application"
	profileevents "github.com/aedst/server/internal/modules/profile/events"
	profilepostgres "github.com/aedst/server/internal/modules/profile/infrastructure/postgres"
	profiletransport "github.com/aedst/server/internal/modules/profile/transport"

	usersapplication "github.com/aedst/server/internal/modules/users/application"
	usersevents "github.com/aedst/server/internal/modules/users/events"
	userspostgres "github.com/aedst/server/internal/modules/users/infrastructure/postgres"
	userstransport "github.com/aedst/server/internal/modules/users/transport"

	platformauth "github.com/aedst/server/internal/platform/auth"
	"github.com/aedst/server/internal/platform/config"
	"github.com/aedst/server/internal/platform/database"
	"github.com/aedst/server/internal/platform/httpserver"
	"github.com/aedst/server/internal/platform/messaging"
	"github.com/aedst/server/internal/platform/observability"
	platformredis "github.com/aedst/server/internal/platform/redis"
	sharedtime "github.com/aedst/server/internal/shared/time"
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

	natsClient, err := messaging.Connect(cfg.NATS.URL)
	if err != nil {
		return err
	}
	defer natsClient.Close()

	if err := natsClient.EnsureStream(ctx, usersevents.Stream, usersevents.Subjects()); err != nil {
		return err
	}
	if err := natsClient.EnsureStream(ctx, profileevents.Stream, profileevents.Subjects()); err != nil {
		return err
	}

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

	// --- auth module ---
	authService := authapplication.NewService(
		authpostgres.NewCredentialRepository(pool),
		authpostgres.NewSessionRepository(pool),
		authtokens.NewAdapter(tokens),
		authsecurity.NewBcryptHasher(),
		clock,
	)
	authHandler := authtransport.NewHandler(authService)

	// --- users module ---
	usersService := usersapplication.NewService(
		userspostgres.NewRepository(pool),
		authService,
		usersevents.NewPublisher(natsClient),
		clock,
		logger,
	)
	usersHandler := userstransport.NewHandler(usersService)

	// --- profile module ---
	profileService := profileapplication.NewService(
		profilepostgres.NewRepository(pool),
		profileevents.NewPublisher(natsClient),
		clock,
	)
	profileHandler := profiletransport.NewHandler(profileService)

	go func() {
		if err := profileevents.ConsumeUserRegistered(ctx, natsClient, profileService); err != nil && !errors.Is(err, context.Canceled) {
			logger.ErrorContext(ctx, "profile UserRegistered consumer stopped", slog.Any("error", err))
		}
	}()

	// Procedures reachable without a token: registration/login/refresh happen
	// before a client has one (see platform/httpserver.AuthInterceptor).
	publicProcedures := map[string]bool{
		authv1connect.AuthServiceLoginProcedure:          true,
		authv1connect.AuthServiceRefreshTokenProcedure:   true,
		authv1connect.AuthServiceLogoutProcedure:         true,
		usersv1connect.UsersServiceRegisterUserProcedure: true,
	}
	interceptors := connect.WithInterceptors(httpserver.AuthInterceptor(tokens, publicProcedures))

	router := httpserver.NewRouter(httpserver.Options{
		Logger:         logger,
		Tracer:         tracer,
		Metrics:        metrics,
		Tokens:         tokens,
		RequestTimeout: 30 * time.Second,
	})

	authPath, authConnectHandler := authv1connect.NewAuthServiceHandler(authHandler, interceptors)
	router.Mount(authPath, authConnectHandler)

	usersPath, usersConnectHandler := usersv1connect.NewUsersServiceHandler(usersHandler, interceptors)
	router.Mount(usersPath, usersConnectHandler)

	profilePath, profileConnectHandler := profilev1connect.NewProfileServiceHandler(profileHandler, interceptors)
	router.Mount(profilePath, profileConnectHandler)

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

	<-ctx.Done()
	logger.Info("shutting down")

	shutdownCtx, cancel := context.WithTimeout(context.Background(), time.Duration(cfg.HTTP.ShutdownTimeout)*time.Second)
	defer cancel()

	_ = server.Shutdown(shutdownCtx)
	_ = metricsServer.Shutdown(shutdownCtx)

	return nil
}
