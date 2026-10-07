#include "aedst/media/media_sources.hpp"

#include <algorithm>
#include <array>
#include <span>

namespace aedst::media {
namespace {

[[nodiscard]] std::span<const mediav1::VariantKind> preferenceFor(DisplayPurpose purpose) {
    static constexpr std::array kThumbnail{mediav1::VARIANT_KIND_THUMBNAIL, mediav1::VARIANT_KIND_POSTER,
                                           mediav1::VARIANT_KIND_PREVIEW, mediav1::VARIANT_KIND_OPTIMIZED};
    static constexpr std::array kPreview{mediav1::VARIANT_KIND_PREVIEW, mediav1::VARIANT_KIND_OPTIMIZED};
    static constexpr std::array kPlayback{mediav1::VARIANT_KIND_OPTIMIZED};
    switch (purpose) {
    case DisplayPurpose::Thumbnail:
        return kThumbnail;
    case DisplayPurpose::Preview:
        return kPreview;
    case DisplayPurpose::Playback:
        break;
    }
    return kPlayback;
}

[[nodiscard]] int longerEdge(const mediav1::Variant& variant) {
    return std::max(variant.width(), variant.height());
}

[[nodiscard]] QString variantKindName(mediav1::VariantKind kind) {
    switch (kind) {
    case mediav1::VARIANT_KIND_THUMBNAIL:
        return QStringLiteral("thumbnail");
    case mediav1::VARIANT_KIND_PREVIEW:
        return QStringLiteral("preview");
    case mediav1::VARIANT_KIND_OPTIMIZED:
        return QStringLiteral("optimized");
    case mediav1::VARIANT_KIND_POSTER:
        return QStringLiteral("poster");
    default:
        break;
    }
    return QStringLiteral("original");
}

} // namespace

const mediav1::Variant* selectVariant(const mediav1::Media& media, DisplayPurpose purpose, int targetPixels) {
    if (media.status() != mediav1::MEDIA_STATUS_READY) {
        return nullptr;
    }
    for (const mediav1::VariantKind kind : preferenceFor(purpose)) {
        const mediav1::Variant* best = nullptr;
        for (const mediav1::Variant& variant : media.variants()) {
            if (variant.kind() != kind || variant.url().empty()) {
                continue;
            }
            if (best == nullptr) {
                best = &variant;
                continue;
            }
            // Prefer the smallest one that still covers the target; otherwise the largest.
            const bool bestCovers = longerEdge(*best) >= targetPixels;
            const bool covers = longerEdge(variant) >= targetPixels;
            if ((covers && (!bestCovers || longerEdge(variant) < longerEdge(*best))) ||
                (!covers && !bestCovers && longerEdge(variant) > longerEdge(*best))) {
                best = &variant;
            }
        }
        if (best != nullptr) {
            return best;
        }
    }
    return nullptr;
}

QUrl urlFor(const mediav1::Media& media, DisplayPurpose purpose, int targetPixels) {
    if (const auto* variant = selectVariant(media, purpose, targetPixels)) {
        return QUrl(QString::fromStdString(variant->url()));
    }
    if (purpose != DisplayPurpose::Thumbnail && media.status() == mediav1::MEDIA_STATUS_READY && !media.url().empty()) {
        return QUrl(QString::fromStdString(media.url()));
    }
    // A READY image without a thumbnail variant: the original still works as a thumbnail.
    if (purpose == DisplayPurpose::Thumbnail && media.kind() == mediav1::MEDIA_KIND_IMAGE &&
        media.status() == mediav1::MEDIA_STATUS_READY && !media.url().empty()) {
        return QUrl(QString::fromStdString(media.url()));
    }
    return {};
}

QString imageSourceFor(const mediav1::Media& media, DisplayPurpose purpose, int targetPixels) {
    if (media.kind() != mediav1::MEDIA_KIND_IMAGE && media.kind() != mediav1::MEDIA_KIND_VIDEO) {
        return {};
    }
    const QUrl url = urlFor(media, purpose == DisplayPurpose::Playback ? DisplayPurpose::Preview : purpose, targetPixels);
    if (url.isEmpty() || (media.kind() == mediav1::MEDIA_KIND_VIDEO && url == QUrl(QString::fromStdString(media.url())))) {
        return {}; // never hand a video file to an image decoder
    }
    const auto* variant = selectVariant(media, purpose, targetPixels);
    const QString kind = variant != nullptr ? variantKindName(variant->kind()) : QStringLiteral("original");
    // The presigned URL is percent-encoded once more so its own '?', '&', '=' and '%'
    // survive QML's url handling; parseImageId() undoes exactly one level.
    const QString encodedUrl = QString::fromLatin1(url.toEncoded(QUrl::FullyEncoded).toPercentEncoding());
    return QStringLiteral("image://media/%1/%2?u=%3").arg(QString::fromStdString(media.id()), kind, encodedUrl);
}

QString kindName(mediav1::MediaKind kind) {
    switch (kind) {
    case mediav1::MEDIA_KIND_IMAGE:
        return QStringLiteral("image");
    case mediav1::MEDIA_KIND_VIDEO:
        return QStringLiteral("video");
    case mediav1::MEDIA_KIND_AUDIO:
        return QStringLiteral("audio");
    case mediav1::MEDIA_KIND_DOCUMENT:
        return QStringLiteral("document");
    default:
        break;
    }
    return {};
}

QString statusName(mediav1::MediaStatus status) {
    switch (status) {
    case mediav1::MEDIA_STATUS_PENDING_UPLOAD:
        return QStringLiteral("pending");
    case mediav1::MEDIA_STATUS_PROCESSING:
        return QStringLiteral("processing");
    case mediav1::MEDIA_STATUS_READY:
        return QStringLiteral("ready");
    case mediav1::MEDIA_STATUS_FAILED:
        return QStringLiteral("failed");
    default:
        break;
    }
    return {};
}

ImageRequest parseImageId(const QString& id) {
    const qsizetype question = id.indexOf(QLatin1Char('?'));
    if (question < 0) {
        return {.cacheKey = id, .url = {}};
    }
    const QString query = id.mid(question + 1);
    if (!query.startsWith(QStringLiteral("u="))) {
        return {.cacheKey = id.left(question), .url = {}};
    }
    const QByteArray once = QByteArray::fromPercentEncoding(query.mid(2).toUtf8());
    QUrl url = QUrl::fromEncoded(once, QUrl::StrictMode);
    // Only network sources: a crafted id must not make the provider read local files.
    if (url.scheme() != QStringLiteral("https") && url.scheme() != QStringLiteral("http")) {
        url = QUrl();
    }
    return {.cacheKey = id.left(question), .url = url};
}

} // namespace aedst::media
