package application

import (
	"context"
	"log/slog"

	authapi "github.com/aedst/server/internal/modules/auth/api"
	"github.com/aedst/server/internal/modules/users/domain"
	sharederrors "github.com/aedst/server/internal/shared/errors"
	"github.com/aedst/server/internal/shared/ids"
	sharedtime "github.com/aedst/server/internal/shared/time"
)

// defaultRoles is assigned at registration until real role management exists
// (see docs/modules.md — RBAC is out of scope for Phase 1).
var defaultRoles = []string{"user"}

type Service struct {
	repo   Repository
	auth   authapi.Service
	events EventPublisher
	clock  sharedtime.Clock
	logger *slog.Logger
}

func NewService(repo Repository, auth authapi.Service, events EventPublisher, clock sharedtime.Clock, logger *slog.Logger) *Service {
	return &Service{repo: repo, auth: auth, events: events, clock: clock, logger: logger}
}

// RegisterUser is the entry point for account creation: it creates the
// credential (via auth's api/, in-process), the core identity record, and
// the first session, then publishes UserRegistered. Credential creation and
// the identity insert are two separate transactions in two schemas (see
// docs/database.md) — if the identity insert fails after the credential
// succeeded, the registration attempt fails and the orphaned credential
// simply blocks that email from registering again; reconciling it is a
// manual/operational task in Phase 1 (an outbox-backed saga is the natural
// upgrade once this proves to be a real problem — see docs/events.md).
func (s *Service) RegisterUser(ctx context.Context, email, password, deviceID string) (domain.User, authapi.TokenPair, error) {
	if _, err := s.repo.FindByEmail(ctx, email); err == nil {
		return domain.User{}, authapi.TokenPair{}, sharederrors.AlreadyExists("users.RegisterUser", "email already registered")
	}

	userID := ids.New()

	if err := s.auth.CreateCredential(ctx, userID, email, password); err != nil {
		return domain.User{}, authapi.TokenPair{}, err
	}

	user := domain.User{
		ID:        userID,
		Email:     email,
		Roles:     defaultRoles,
		CreatedAt: s.clock.Now(),
	}
	if err := s.repo.Create(ctx, user); err != nil {
		return domain.User{}, authapi.TokenPair{}, err
	}

	pair, err := s.auth.IssueSession(ctx, userID, deviceID)
	if err != nil {
		return domain.User{}, authapi.TokenPair{}, err
	}

	// Best-effort: a failed publish doesn't fail registration. Consumers
	// (profile, notifications) not seeing this event is a gap to close with
	// an outbox once it's observed in practice (see docs/events.md).
	if err := s.events.PublishUserRegistered(ctx, user); err != nil {
		s.logger.ErrorContext(ctx, "failed to publish UserRegistered", slog.String("user_id", userID.String()), slog.Any("error", err))
	}

	return user, pair, nil
}

func (s *Service) GetUser(ctx context.Context, id ids.ID) (domain.User, error) {
	return s.repo.FindByID(ctx, id)
}

func (s *Service) GetUserByEmail(ctx context.Context, email string) (domain.User, error) {
	return s.repo.FindByEmail(ctx, email)
}
