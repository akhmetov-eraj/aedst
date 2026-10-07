package processing

import (
	"bytes"
	"context"
	"errors"
	"fmt"
	"io"
	"log/slog"
	"os"
	"time"

	"github.com/riverqueue/river"

	"github.com/aedst/server/internal/modules/media/domain"
	mediajobs "github.com/aedst/server/internal/modules/media/infrastructure/jobs"
	"github.com/aedst/server/internal/platform/storage"
	"github.com/aedst/server/internal/shared/ids"
)

// Store is what the processing worker needs from the repository.
type Store interface {
	GetByID(ctx context.Context, id ids.ID) (domain.Media, error)
	MarkReady(ctx context.Context, id ids.ID, width, height int, durationMs int64, variants []domain.Variant) error
	MarkFailed(ctx context.Context, id ids.ID, reason string, now time.Time) error
}

// Config bounds processing.
type Config struct {
	MaxImagePixels int64
	FFmpeg         FFmpeg
}

// Worker runs mediajobs.ProcessArgs on the "media" queue.
type Worker struct {
	river.WorkerDefaults[mediajobs.ProcessArgs]
	store   Store
	objects storage.ObjectStorage
	cfg     Config
	logger  *slog.Logger
}

func NewWorker(store Store, objects storage.ObjectStorage, cfg Config, logger *slog.Logger) *Worker {
	return &Worker{store: store, objects: objects, cfg: cfg, logger: logger}
}

// Timeout covers download, decode/ffmpeg and variant uploads.
func (w *Worker) Timeout(*river.Job[mediajobs.ProcessArgs]) time.Duration { return 10 * time.Minute }

func (w *Worker) Work(ctx context.Context, job *river.Job[mediajobs.ProcessArgs]) error {
	id, err := ids.Parse(job.Args.MediaID)
	if err != nil {
		return river.JobCancel(err)
	}
	m, err := w.store.GetByID(ctx, id)
	if err != nil {
		return err
	}
	if m.Status != domain.StatusProcessing {
		return nil // already done (retry after commit) or deleted/failed
	}

	err = w.process(ctx, m)
	switch {
	case err == nil:
		return nil
	case errors.Is(err, ErrUnprocessable):
		w.logger.InfoContext(ctx, "media: unprocessable upload", slog.String("media_id", id.String()), slog.Any("reason", err))
		if markErr := w.store.MarkFailed(ctx, id, "the file could not be processed", time.Now()); markErr != nil {
			return markErr
		}
		return river.JobCancel(err)
	default:
		if job.Attempt >= job.MaxAttempts {
			_ = w.store.MarkFailed(ctx, id, "processing failed", time.Now())
		}
		return err
	}
}

func (w *Worker) process(ctx context.Context, m domain.Media) error {
	if m.Kind == domain.KindDocument || (m.Kind != domain.KindImage && !w.cfg.FFmpeg.Enabled()) {
		return w.store.MarkReady(ctx, m.ID, 0, 0, 0, nil)
	}
	path, err := w.download(ctx, m)
	if err != nil {
		return err
	}
	defer func() { _ = os.Remove(path) }()

	switch m.Kind {
	case domain.KindImage:
		f, err := os.Open(path)
		if err != nil {
			return fmt.Errorf("media: open: %w", err)
		}
		defer func() { _ = f.Close() }()
		width, height, encoded, err := ImageVariants(f, m.ContentType, w.cfg.MaxImagePixels)
		if err != nil {
			return err
		}
		variants, err := w.storeVariants(ctx, m, encoded)
		if err != nil {
			return err
		}
		return w.store.MarkReady(ctx, m.ID, width, height, 0, variants)
	case domain.KindVideo:
		probe, err := w.cfg.FFmpeg.Probe(ctx, path)
		if err != nil {
			return err
		}
		encoded, err := w.cfg.FFmpeg.Poster(ctx, path, probe.DurationMs)
		if err != nil {
			return err
		}
		variants, err := w.storeVariants(ctx, m, encoded)
		if err != nil {
			return err
		}
		return w.store.MarkReady(ctx, m.ID, probe.Width, probe.Height, probe.DurationMs, variants)
	default: // audio
		probe, err := w.cfg.FFmpeg.Probe(ctx, path)
		if err != nil {
			return err
		}
		return w.store.MarkReady(ctx, m.ID, 0, 0, probe.DurationMs, nil)
	}
}

// download copies the original to a temp file, never reading more than the
// verified size (+1 to detect a changed object).
func (w *Worker) download(ctx context.Context, m domain.Media) (string, error) {
	body, _, err := w.objects.Get(ctx, m.ObjectKey)
	if errors.Is(err, storage.ErrNotFound) {
		return "", fmt.Errorf("%w: original object is gone", ErrUnprocessable)
	}
	if err != nil {
		return "", fmt.Errorf("media: download: %w", err)
	}
	defer func() { _ = body.Close() }()
	f, err := os.CreateTemp("", "aedst-media-*")
	if err != nil {
		return "", fmt.Errorf("media: temp: %w", err)
	}
	n, copyErr := io.Copy(f, io.LimitReader(body, m.SizeBytes+1))
	closeErr := f.Close()
	if copyErr != nil || closeErr != nil {
		_ = os.Remove(f.Name())
		return "", fmt.Errorf("media: download: %w", errors.Join(copyErr, closeErr))
	}
	if n != m.SizeBytes {
		_ = os.Remove(f.Name())
		return "", fmt.Errorf("%w: object size changed after verification", ErrUnprocessable)
	}
	return f.Name(), nil
}

// storeVariants uploads encoded variants under the item's prefix.
func (w *Worker) storeVariants(ctx context.Context, m domain.Media, encoded []Encoded) ([]domain.Variant, error) {
	out := make([]domain.Variant, 0, len(encoded))
	for _, e := range encoded {
		key := domain.ObjectKey(m.OwnerID, m.ID, e.Name+"."+e.Ext)
		if err := w.objects.Put(ctx, key, bytes.NewReader(e.Data), int64(len(e.Data)), e.ContentType); err != nil {
			return nil, fmt.Errorf("media: store variant: %w", err)
		}
		out = append(out, domain.Variant{
			Kind: domain.VariantKind(e.Name), ObjectKey: key, ContentType: e.ContentType,
			Width: e.Width, Height: e.Height, SizeBytes: int64(len(e.Data)),
		})
	}
	return out, nil
}

// DeleteWorker runs mediajobs.DeleteObjectsArgs.
type DeleteWorker struct {
	river.WorkerDefaults[mediajobs.DeleteObjectsArgs]
	objects storage.ObjectStorage
}

func NewDeleteWorker(objects storage.ObjectStorage) *DeleteWorker {
	return &DeleteWorker{objects: objects}
}

func (w *DeleteWorker) Work(ctx context.Context, job *river.Job[mediajobs.DeleteObjectsArgs]) error {
	var errs []error
	for _, key := range job.Args.Keys {
		if err := w.objects.Delete(ctx, key); err != nil && !errors.Is(err, storage.ErrNotFound) {
			errs = append(errs, err)
		}
	}
	return errors.Join(errs...)
}
