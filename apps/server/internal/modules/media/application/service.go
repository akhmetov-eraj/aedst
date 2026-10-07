package application

import (
	"context"
	"encoding/hex"
	"errors"
	"io"
	"net/http"

	"github.com/aedst/server/internal/modules/media/domain"
	"github.com/aedst/server/internal/platform/storage"
	sharederrors "github.com/aedst/server/internal/shared/errors"
	"github.com/aedst/server/internal/shared/ids"
	"github.com/aedst/server/internal/shared/pagination"
	sharedtime "github.com/aedst/server/internal/shared/time"
)

// sniffBytes is what http.DetectContentType looks at.
const sniffBytes = 512

type Service struct {
	repo    Repository
	store   ObjectStorage
	tasks   TaskOwnership
	clock   sharedtime.Clock
	cfg     Config
	enabled bool
}

// NewService builds the media use cases. enabled=false (no bucket
// configured) makes every RPC fail with FAILED_PRECONDITION.
func NewService(repo Repository, store ObjectStorage, tasks TaskOwnership, clock sharedtime.Clock, cfg Config, enabled bool) *Service {
	return &Service{repo: repo, store: store, tasks: tasks, clock: clock, cfg: cfg, enabled: enabled}
}

var errDisabled = sharederrors.FailedPrecondition("media", "media uploads are not configured on this server")

// CreateUpload validates the request, records a PENDING_UPLOAD row and
// returns a presigned PUT whose signature covers content type, length and
// (when given) the SHA-256, so the client cannot upload something else.
func (s *Service) CreateUpload(ctx context.Context, owner ids.ID, filename, contentType string, size int64, sha256Hex string, taskID *ids.ID) (Upload, error) {
	if !s.enabled {
		return Upload{}, errDisabled
	}
	if taskID != nil {
		ok, err := s.tasks.OwnsTask(ctx, owner, *taskID)
		if err != nil {
			return Upload{}, err
		}
		if !ok {
			// Same answer for "not yours" and "does not exist".
			return Upload{}, sharederrors.NotFound("media.CreateUpload", "task not found")
		}
	}
	m, err := domain.NewUpload(ids.New(), owner, filename, contentType, size, sha256Hex, taskID, s.cfg.Limits, s.clock.Now())
	if err != nil {
		return Upload{}, err
	}
	var sum []byte
	if m.SHA256 != "" {
		sum, _ = hex.DecodeString(m.SHA256)
	}
	req, err := s.store.CreateUploadURL(ctx, storage.UploadRequest{Key: m.ObjectKey, ContentType: m.ContentType, Size: m.DeclaredSize, SHA256: sum, TTL: s.cfg.UploadURLTTL})
	if err != nil {
		return Upload{}, sharederrors.Wrap("media.CreateUpload", err)
	}
	created, err := s.repo.Create(ctx, m)
	if err != nil {
		return Upload{}, err
	}
	return Upload{Media: created, Request: req}, nil
}

// CompleteUpload verifies the uploaded object (exists, exact declared size,
// sniffed content type consistent with the declared one) and starts
// processing. Idempotent: completing an item that is no longer pending
// returns it unchanged. A verification failure marks the item FAILED, deletes
// the object and returns the FAILED item.
func (s *Service) CompleteUpload(ctx context.Context, owner, id ids.ID) (domain.Media, error) {
	if !s.enabled {
		return domain.Media{}, errDisabled
	}
	m, err := s.repo.Get(ctx, owner, id)
	if err != nil {
		return domain.Media{}, err
	}
	if m.Status != domain.StatusPendingUpload {
		return m, nil
	}
	info, err := s.store.Head(ctx, m.ObjectKey)
	if errors.Is(err, storage.ErrNotFound) {
		return domain.Media{}, sharederrors.FailedPrecondition("media.CompleteUpload", "the file has not been uploaded yet")
	}
	if err != nil {
		return domain.Media{}, sharederrors.Wrap("media.CompleteUpload", err)
	}
	if info.Size != m.DeclaredSize {
		return s.reject(ctx, m, "uploaded size does not match the declared size")
	}
	sniffed, err := s.sniff(ctx, m.ObjectKey)
	if err != nil {
		return domain.Media{}, err
	}
	if !domain.SniffMatches(m.ContentType, sniffed) {
		return s.reject(ctx, m, "file content does not match its declared type")
	}
	started, ok, err := s.repo.StartProcessing(ctx, owner, id, info.Size, s.clock.Now())
	if err != nil {
		return domain.Media{}, err
	}
	if !ok {
		return s.repo.Get(ctx, owner, id)
	}
	return started, nil
}

func (s *Service) sniff(ctx context.Context, key string) (string, error) {
	body, _, err := s.store.Get(ctx, key)
	if err != nil {
		return "", sharederrors.Wrap("media.sniff", err)
	}
	defer func() { _ = body.Close() }()
	buf := make([]byte, sniffBytes)
	n, err := io.ReadFull(body, buf)
	if err != nil && !errors.Is(err, io.ErrUnexpectedEOF) && !errors.Is(err, io.EOF) {
		return "", sharederrors.Wrap("media.sniff", err)
	}
	return http.DetectContentType(buf[:n]), nil
}

func (s *Service) reject(ctx context.Context, m domain.Media, reason string) (domain.Media, error) {
	if err := s.repo.MarkFailed(ctx, m.ID, reason, s.clock.Now()); err != nil {
		return domain.Media{}, err
	}
	if err := s.store.Delete(ctx, m.ObjectKey); err != nil {
		// The pending-upload retention job would not see a FAILED row; the
		// object would leak, but correctness is unaffected. Report it.
		return domain.Media{}, sharederrors.Wrap("media.reject", err)
	}
	return s.repo.Get(ctx, m.OwnerID, m.ID)
}

func (s *Service) GetMedia(ctx context.Context, owner, id ids.ID) (domain.Media, error) {
	if !s.enabled {
		return domain.Media{}, errDisabled
	}
	return s.repo.Get(ctx, owner, id)
}

func (s *Service) ListMedia(ctx context.Context, owner ids.ID, taskID *ids.ID, kinds []domain.Kind, pageSize int32, token string) (ListResult, error) {
	if !s.enabled {
		return ListResult{}, errDisabled
	}
	after, err := pagination.Decode(token, SortToken)
	if err != nil {
		return ListResult{}, sharederrors.Validation("media.ListMedia", "invalid page token")
	}
	return s.repo.List(ctx, ListParams{OwnerID: owner, TaskID: taskID, Kinds: kinds, Limit: int(pagination.ClampLimit(pageSize)), After: after})
}

func (s *Service) DeleteMedia(ctx context.Context, owner, id ids.ID) error {
	if !s.enabled {
		return errDisabled
	}
	return s.repo.Delete(ctx, owner, id, s.clock.Now())
}

// Links presigns download URLs for a READY item (empty otherwise).
func (s *Service) Links(ctx context.Context, m domain.Media) (Links, error) {
	if !s.enabled || m.Status != domain.StatusReady {
		return Links{}, nil
	}
	original, err := s.store.CreateDownloadURL(ctx, m.ObjectKey, s.cfg.DownloadURLTTL, m.Filename)
	if err != nil {
		return Links{}, sharederrors.Wrap("media.Links", err)
	}
	out := Links{Original: original, Variants: map[domain.VariantKind]string{}}
	for _, v := range m.Variants {
		u, err := s.store.CreateDownloadURL(ctx, v.ObjectKey, s.cfg.DownloadURLTTL, "")
		if err != nil {
			return Links{}, sharederrors.Wrap("media.Links", err)
		}
		out.Variants[v.Kind] = u
	}
	return out, nil
}

// Totals feeds the operator overview.
func (s *Service) Totals(ctx context.Context) (count, bytes int64, err error) {
	return s.repo.Totals(ctx)
}
