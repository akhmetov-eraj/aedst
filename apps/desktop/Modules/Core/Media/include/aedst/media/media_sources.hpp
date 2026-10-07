#pragma once

#include <aedst/media/v1/media.pb.h>

#include <QString>
#include <QUrl>

namespace aedst::media {

namespace mediav1 = ::aedst::media::v1;

/// What a view is about to draw; decides which server variant fits.
enum class DisplayPurpose {
    /// Grid cells / attachment chips (<= ~256 px).
    Thumbnail,
    /// Detail view / lightbox (screen-sized).
    Preview,
    /// Video playback.
    Playback,
};

/// The smallest READY rendition that still covers `targetPixels` on its longer
/// edge, preferring the variant kind meant for `purpose`:
///  - Thumbnail: THUMBNAIL, then POSTER (videos), PREVIEW, OPTIMIZED;
///  - Preview:   PREVIEW, OPTIMIZED, then the original;
///  - Playback:  OPTIMIZED, then the original.
/// Returns nullptr when only the original exists (or nothing is ready); then use
/// media.url() for Preview/Playback, or a local fallback thumbnail.
[[nodiscard]] const mediav1::Variant* selectVariant(const mediav1::Media& media, DisplayPurpose purpose,
                                                    int targetPixels);

/// URL a view loads for `purpose`: a variant's presigned URL, the original, or empty.
[[nodiscard]] QUrl urlFor(const mediav1::Media& media, DisplayPurpose purpose, int targetPixels);

/// Image source for MediaImageProvider: `image://media/<mediaId>/<kind>?u=<url>`.
/// Presigned URLs rotate on every fetch, so the provider caches by
/// `<mediaId>/<kind>` (stable) and only uses `u` to download on a cache miss.
/// Empty when there is nothing displayable (not READY, or not an image/video).
[[nodiscard]] QString imageSourceFor(const mediav1::Media& media, DisplayPurpose purpose, int targetPixels);

/// "image" | "video" | "audio" | "document" | "" - QML-friendly kind names.
[[nodiscard]] QString kindName(mediav1::MediaKind kind);
/// "pending" | "processing" | "ready" | "failed" | "".
[[nodiscard]] QString statusName(mediav1::MediaStatus status);

/// Parses an `image://media/...` id (the part after the provider name).
struct ImageRequest {
    QString cacheKey;
    QUrl url;
};
[[nodiscard]] ImageRequest parseImageId(const QString& id);

} // namespace aedst::media
