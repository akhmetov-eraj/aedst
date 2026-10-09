package config_test

import (
	"strings"
	"testing"

	"github.com/aedst/server/internal/platform/config"
)

// An explicitly empty AEDST_JOBS_QUEUES makes the process insert-only (an API
// replica with workers deployed apart); only an unset variable takes the default.
func TestLoad_EmptyJobsQueuesIsInsertOnly(t *testing.T) {
	t.Setenv("AEDST_DATABASE_URL", "postgres://x")
	t.Setenv("AEDST_JWT_SIGNING_KEY", strings.Repeat("k", 40))
	t.Setenv("AEDST_AUTH_FIRST_PARTY_ENABLED", "true")
	t.Setenv("AEDST_JOBS_QUEUES", "")
	cfg, err := config.Load()
	if err != nil {
		t.Fatalf("Load() error = %v", err)
	}
	if len(cfg.Jobs.Queues) != 0 {
		t.Fatalf("Queues = %v, want empty (insert-only)", cfg.Jobs.Queues)
	}
}
