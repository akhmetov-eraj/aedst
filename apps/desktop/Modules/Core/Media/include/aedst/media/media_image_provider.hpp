#pragma once

#include <QCache>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QQuickAsyncImageProvider>
#include <QSize>
#include <QString>
#include <QUrl>

#include <functional>
#include <memory>

class QNetworkAccessManager;
class QNetworkDiskCache;

namespace aedst::media {

/// Lives on the GUI thread and owns the network side of image loading (Qt Network
/// objects must be used from their own thread).
class MediaImageFetcher final : public QObject {
    Q_OBJECT

public:
    /// `cacheDirectory` backs a QNetworkDiskCache (bounded, LRU-ish by Qt).
    MediaImageFetcher(const QString& cacheDirectory, qint64 maxDiskCacheBytes, QObject* parent = nullptr);
    ~MediaImageFetcher() override;
    MediaImageFetcher(const MediaImageFetcher&) = delete;
    MediaImageFetcher& operator=(const MediaImageFetcher&) = delete;
    MediaImageFetcher(MediaImageFetcher&&) = delete;
    MediaImageFetcher& operator=(MediaImageFetcher&&) = delete;

    /// Downloads `url` and calls `done(bytes, error)` on the GUI thread.
    /// Thread-safe to call: it queues onto the fetcher's thread.
    void fetch(const QUrl& url, std::function<void(QByteArray, QString)> done);

private:
    std::unique_ptr<QNetworkAccessManager> network_;
};

/// `image://media/<mediaId>/<variant>?u=<presigned url>` for QML `Image` elements
/// (use `asynchronous: true` and a `sourceSize`):
///  - async: decoding runs on a worker thread (QQuickImageResponse), downloads
///    on the GUI thread's QNetworkAccessManager - the UI never blocks;
///  - cache: a process-wide in-memory QCache keyed by `<mediaId>/<variant>@<size>`
///    (presigned URLs rotate, the key does not), plus a QNetworkDiskCache for the
///    bytes while a URL stays valid;
///  - sizing: decodes straight to `requestedSize` with QImageReader::setScaledSize.
/// Only http(s) sources are fetched (see parseImageId). Register once:
/// `engine.addImageProvider("media", new MediaImageProvider(fetcher))` (the engine
/// takes ownership of the provider, not of the fetcher).
class MediaImageProvider final : public QQuickAsyncImageProvider {
public:
    static constexpr qsizetype kDefaultMemoryCacheKiB = 64 * 1024;

    explicit MediaImageProvider(MediaImageFetcher& fetcher, qsizetype memoryCacheKiB = kDefaultMemoryCacheKiB);

    QQuickImageResponse* requestImageResponse(const QString& id, const QSize& requestedSize) override;

    /// Thread-safe cache access for responses.
    [[nodiscard]] QImage cached(const QString& key) const;
    void store(const QString& key, const QImage& image);

private:
    MediaImageFetcher& fetcher_;
    mutable QMutex mutex_;
    QCache<QString, QImage> memory_;
};

} // namespace aedst::media
