package processing_test

import (
	"bytes"
	"context"
	"errors"
	"image"
	"image/color"
	"image/jpeg"
	"image/png"
	"log/slog"
	"os/exec"
	"testing"
	"time"

	"github.com/riverqueue/river"
	"github.com/riverqueue/river/rivertype"

	"github.com/aedst/server/internal/modules/media/domain"
	mediajobs "github.com/aedst/server/internal/modules/media/infrastructure/jobs"
	"github.com/aedst/server/internal/modules/media/infrastructure/processing"
	"github.com/aedst/server/internal/platform/storage"
	"github.com/aedst/server/internal/shared/ids"
)

func encodeImage(t *testing.T, w, h int, alpha bool) []byte {
	t.Helper()
	img := image.NewNRGBA(image.Rect(0, 0, w, h))
	for y := range h {
		for x := range w {
			a := uint8(255)
			if alpha && x == 0 {
				a = 0
			}
			img.SetNRGBA(x, y, color.NRGBA{R: uint8(x), G: uint8(y), B: 100, A: a})
		}
	}
	var buf bytes.Buffer
	if alpha {
		_ = png.Encode(&buf, img)
	} else {
		_ = jpeg.Encode(&buf, img, &jpeg.Options{Quality: 95})
	}
	return buf.Bytes()
}

func TestImageVariants(t *testing.T) {
	w, h, out, err := processing.ImageVariants(bytes.NewReader(encodeImage(t, 3000, 1500, false)), "image/jpeg", 50_000_000)
	if err != nil {
		t.Fatal(err)
	}
	if w != 3000 || h != 1500 || len(out) != 3 {
		t.Fatalf("got %dx%d, %d variants", w, h, len(out))
	}
	want := map[string][2]int{"thumbnail": {256, 128}, "preview": {1280, 640}, "optimized": {2560, 1280}}
	for _, v := range out {
		if dims := want[v.Name]; v.Width != dims[0] || v.Height != dims[1] || v.ContentType != "image/jpeg" {
			t.Errorf("%s = %dx%d %s", v.Name, v.Width, v.Height, v.ContentType)
		}
	}

	_, _, out, err = processing.ImageVariants(bytes.NewReader(encodeImage(t, 64, 32, true)), "image/png", 50_000_000)
	if err != nil || out[0].ContentType != "image/png" || out[0].Width != 64 {
		t.Fatalf("transparent png must stay png and never be upscaled: %+v, %v", out[0], err)
	}
}

func TestImageVariants_RefusesBombsAndGarbage(t *testing.T) {
	if _, _, _, err := processing.ImageVariants(bytes.NewReader(encodeImage(t, 100, 100, false)), "image/jpeg", 5_000); !errors.Is(err, processing.ErrUnprocessable) {
		t.Errorf("over the pixel limit: %v", err)
	}
	if _, _, _, err := processing.ImageVariants(bytes.NewReader([]byte("not an image")), "image/png", 1e9); !errors.Is(err, processing.ErrUnprocessable) {
		t.Errorf("garbage: %v", err)
	}
}

type fakeStore struct {
	m        domain.Media
	ready    bool
	variants []domain.Variant
	failed   string
}

func (f *fakeStore) GetByID(context.Context, ids.ID) (domain.Media, error) { return f.m, nil }
func (f *fakeStore) MarkReady(_ context.Context, _ ids.ID, _, _ int, _ int64, v []domain.Variant) error {
	f.ready, f.variants = true, v
	return nil
}

func (f *fakeStore) MarkFailed(_ context.Context, _ ids.ID, reason string, _ time.Time) error {
	f.failed = reason
	return nil
}

func run(t *testing.T, w *processing.Worker, id ids.ID) error {
	t.Helper()
	return w.Work(context.Background(), &river.Job[mediajobs.ProcessArgs]{JobRow: &rivertype.JobRow{Attempt: 1, MaxAttempts: 5}, Args: mediajobs.ProcessArgs{MediaID: id.String()}})
}

func TestWorker_ImageEndToEndAndUnprocessable(t *testing.T) {
	objects := storage.NewMemory()
	owner, id := ids.New(), ids.New()
	data := encodeImage(t, 600, 300, false)
	key := domain.ObjectKey(owner, id, "original")
	_ = objects.Put(context.Background(), key, bytes.NewReader(data), int64(len(data)), "image/jpeg")
	store := &fakeStore{m: domain.Media{ID: id, OwnerID: owner, Kind: domain.KindImage, Status: domain.StatusProcessing, ContentType: "image/jpeg", ObjectKey: key, SizeBytes: int64(len(data))}}
	w := processing.NewWorker(store, objects, processing.Config{MaxImagePixels: 50_000_000}, slog.New(slog.DiscardHandler))

	if err := run(t, w, id); err != nil || !store.ready || len(store.variants) != 3 {
		t.Fatalf("Work = %v, ready %v, variants %d", err, store.ready, len(store.variants))
	}
	if len(objects.Keys()) != 4 {
		t.Errorf("stored objects = %v, want original + 3 variants", objects.Keys())
	}

	store.m.Status = domain.StatusReady
	store.ready = false
	if err := run(t, w, id); err != nil || store.ready {
		t.Fatal("a retried job after success must do nothing")
	}

	_ = objects.Put(context.Background(), key, bytes.NewReader([]byte("garbage!")), 8, "image/jpeg")
	store.m.Status, store.m.SizeBytes = domain.StatusProcessing, 8
	err := run(t, w, id)
	var cancel *rivertype.JobCancelError
	if !errors.As(err, &cancel) || store.failed == "" {
		t.Fatalf("garbage: err %v, failed %q; want FAILED and a cancelled job", err, store.failed)
	}
}

// Runs only where ffmpeg/ffprobe are installed (CI images may not have them).
func TestFFmpeg_PosterAndProbe(t *testing.T) {
	ffmpeg, err1 := exec.LookPath("ffmpeg")
	ffprobe, err2 := exec.LookPath("ffprobe")
	if err1 != nil || err2 != nil {
		t.Skip("ffmpeg/ffprobe not installed")
	}
	dir := t.TempDir()
	clip := dir + "/clip.mp4"
	if out, err := exec.Command(ffmpeg, "-v", "error", "-f", "lavfi", "-i", "testsrc=duration=3:size=320x240:rate=10", "-pix_fmt", "yuv420p", clip).CombinedOutput(); err != nil {
		t.Skipf("cannot synthesize a clip: %v %s", err, out)
	}
	f := processing.FFmpeg{FFmpegPath: ffmpeg, FFprobePath: ffprobe, Timeout: 30 * time.Second}
	probe, err := f.Probe(context.Background(), clip)
	if err != nil || probe.Width != 320 || probe.Height != 240 || probe.DurationMs < 2500 {
		t.Fatalf("probe = %+v, %v", probe, err)
	}
	variants, err := f.Poster(context.Background(), clip, probe.DurationMs)
	if err != nil || len(variants) != 2 || variants[0].Name != "poster" || variants[0].Width != 320 {
		t.Fatalf("poster = %+v, %v", variants, err)
	}
}
