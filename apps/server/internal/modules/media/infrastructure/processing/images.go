// Package processing turns a completed upload into its variants: thumbnails,
// previews and re-encoded (compressed) renditions for images, and a poster
// frame plus metadata for video/audio when ffmpeg is available.
package processing

import (
	"bytes"
	"errors"
	"fmt"
	"image"
	"image/draw"
	"image/gif"
	"image/jpeg"
	"image/png"
	"io"

	xdraw "golang.org/x/image/draw"
	"golang.org/x/image/webp"
)

// ErrUnprocessable marks input that will never process (corrupt, too large,
// unsupported): the item is marked FAILED and the job is not retried.
var ErrUnprocessable = errors.New("media: unprocessable input")

// rendition is one image variant's target bounding box and JPEG quality.
type rendition struct {
	name    string
	maxSide int
	quality int
}

// Encoded is an encoded variant ready to store.
type Encoded struct {
	Name        string
	ContentType string
	Ext         string
	Width       int
	Height      int
	Data        []byte
}

var imageRenditions = []rendition{
	{name: "thumbnail", maxSide: 256, quality: 75},
	{name: "preview", maxSide: 1280, quality: 80},
	{name: "optimized", maxSide: 2560, quality: 85},
}

// decodeImage decodes by declared type after checking dimensions, so a
// decompression bomb (tiny file, huge canvas) is refused before allocation.
func decodeImage(r io.ReadSeeker, contentType string, maxPixels int64) (image.Image, error) {
	var (
		decodeConfig func(io.Reader) (image.Config, error)
		decode       func(io.Reader) (image.Image, error)
	)
	switch contentType {
	case "image/jpeg":
		decodeConfig, decode = jpeg.DecodeConfig, jpeg.Decode
	case "image/png":
		decodeConfig, decode = png.DecodeConfig, png.Decode
	case "image/gif":
		decodeConfig, decode = gif.DecodeConfig, gif.Decode // first frame
	case "image/webp":
		decodeConfig, decode = webp.DecodeConfig, webp.Decode
	default:
		return nil, fmt.Errorf("%w: unsupported image type %s", ErrUnprocessable, contentType)
	}
	cfg, err := decodeConfig(r)
	if err != nil {
		return nil, fmt.Errorf("%w: %w", ErrUnprocessable, err)
	}
	if cfg.Width <= 0 || cfg.Height <= 0 || int64(cfg.Width)*int64(cfg.Height) > maxPixels {
		return nil, fmt.Errorf("%w: image is %dx%d (limit %d pixels)", ErrUnprocessable, cfg.Width, cfg.Height, maxPixels)
	}
	if _, err := r.Seek(0, io.SeekStart); err != nil {
		return nil, fmt.Errorf("media: rewind: %w", err)
	}
	img, err := decode(r)
	if err != nil {
		return nil, fmt.Errorf("%w: %w", ErrUnprocessable, err)
	}
	return img, nil
}

// ImageVariants decodes src and produces every rendition. Opaque images are
// re-encoded as JPEG (that is the compression step); images with
// transparency stay PNG so the alpha channel survives.
func ImageVariants(src io.ReadSeeker, contentType string, maxPixels int64) (width, height int, out []Encoded, err error) {
	img, err := decodeImage(src, contentType, maxPixels)
	if err != nil {
		return 0, 0, nil, err
	}
	b := img.Bounds()
	alpha := hasAlpha(img)
	for _, r := range imageRenditions {
		scaled := fit(img, r.maxSide)
		enc, err := encode(scaled, alpha, r.quality)
		if err != nil {
			return 0, 0, nil, err
		}
		enc.Name = r.name
		out = append(out, enc)
	}
	return b.Dx(), b.Dy(), out, nil
}

// Thumbnail makes a JPEG thumbnail of img (video posters).
func Thumbnail(img image.Image) (Encoded, error) {
	enc, err := encode(fit(img, imageRenditions[0].maxSide), false, imageRenditions[0].quality)
	enc.Name = "thumbnail"
	return enc, err
}

// fit scales img down (never up) to fit maxSide x maxSide.
func fit(img image.Image, maxSide int) image.Image {
	b := img.Bounds()
	w, h := b.Dx(), b.Dy()
	if w <= maxSide && h <= maxSide {
		return img
	}
	if w >= h {
		h = max(1, h*maxSide/w)
		w = maxSide
	} else {
		w = max(1, w*maxSide/h)
		h = maxSide
	}
	dst := image.NewRGBA(image.Rect(0, 0, w, h))
	xdraw.CatmullRom.Scale(dst, dst.Bounds(), img, b, draw.Src, nil)
	return dst
}

func encode(img image.Image, alpha bool, quality int) (Encoded, error) {
	var buf bytes.Buffer
	b := img.Bounds()
	if alpha {
		enc := png.Encoder{CompressionLevel: png.BestCompression}
		if err := enc.Encode(&buf, img); err != nil {
			return Encoded{}, fmt.Errorf("media: encode png: %w", err)
		}
		return Encoded{ContentType: "image/png", Ext: "png", Width: b.Dx(), Height: b.Dy(), Data: buf.Bytes()}, nil
	}
	if err := jpeg.Encode(&buf, img, &jpeg.Options{Quality: quality}); err != nil {
		return Encoded{}, fmt.Errorf("media: encode jpeg: %w", err)
	}
	return Encoded{ContentType: "image/jpeg", Ext: "jpg", Width: b.Dx(), Height: b.Dy(), Data: buf.Bytes()}, nil
}

// hasAlpha reports whether any pixel is not fully opaque (sampled on a grid
// for large images; exact for small ones).
func hasAlpha(img image.Image) bool {
	if o, ok := img.(interface{ Opaque() bool }); ok {
		return !o.Opaque()
	}
	b := img.Bounds()
	step := max(1, max(b.Dx(), b.Dy())/512)
	for y := b.Min.Y; y < b.Max.Y; y += step {
		for x := b.Min.X; x < b.Max.X; x += step {
			if _, _, _, a := img.At(x, y).RGBA(); a != 0xffff {
				return true
			}
		}
	}
	return false
}
