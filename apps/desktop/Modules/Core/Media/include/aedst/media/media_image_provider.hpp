#pragma once

#include <QCache>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QQuickAsyncImageProvider>
#include <QSize>
#include <QString>
#include <QUrl>

#include <atomic>
#include <functional>
#include <memory>

class QNetworkAccessManager;
class QNetworkReply;
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

    /// One download in flight. `cancelled` may be set from any thread; `reply` belongs to the fetcher's thread.
    struct Ticket {
        std::atomic<bool> cancelled{false};
        QPointer<QNetworkReply> reply;
    };

    /// Downloads `url` and calls `done(bytes, error)` on the GUI thread, exactly once - also when the download is
    /// cancelled (then with an error). Thread-safe to call: it queues onto the fetcher's thread. A ticket cancelled
    /// before the request starts never touches the network.
    std::shared_ptr<Ticket> fetch(const QUrl& url, std::function<void(QByteArray, QString)> done);

    /// Stops the download of `ticket` (an aborted transfer, or none at all if it has not started). Thread-safe.
    void cancel(const std::shared_ptr<Ticket>& ticket);

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
