package main

import (
	"context"
	"log/slog"
	"time"

	"github.com/aedst/server/internal/platform/config"
)

// logFirstPartySunset logs, on every start, that first-party auth is a
// time-boxed migration path and how long it has left. A passed sunset is a
// startup error in staging/production (config.Validate); in development it is
// only logged, loudly.
func logFirstPartySunset(ctx context.Context, logger *slog.Logger, cfg config.Config) {
	days, expired, ok := cfg.Auth.SunsetStatus(time.Now())
	switch {
	case !ok:
		logger.WarnContext(ctx, "first-party auth is ENABLED (AEDST_AUTH_FIRST_PARTY_ENABLED=true) without a sunset date; it is a temporary migration path to the identity provider, set AEDST_AUTH_FIRST_PARTY_SUNSET")
	case expired:
		logger.ErrorContext(ctx, "first-party auth SUNSET HAS PASSED: complete the Sunset runbook (docs/content/server/runbook.md), migrate users to the identity provider and set AEDST_AUTH_FIRST_PARTY_ENABLED=false",
			slog.String("sunset", cfg.Auth.FirstPartySunset), slog.Int("days_overdue", -days))
	default:
		logger.WarnContext(ctx, "first-party auth is ENABLED as a temporary migration path and will be removed",
			slog.String("sunset", cfg.Auth.FirstPartySunset), slog.Int("days_remaining", days))
	}
}
