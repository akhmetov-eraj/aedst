#include "aedst/media/media_image_provider.hpp"

#include "aedst/media/media_sources.hpp"

#include <QBuffer>
#include <QImageReader>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QQuickTextureFactory>
#include <QThreadPool>

#include <algorithm>
#include <utility>

namespace aedst::media {
namespace {

constexpr qint64 kMaxImageBytes = 40LL * 1024 * 1024;

[[nodiscard]] QString memoryKey(const QString& cacheKey, const QSize& requestedSize) {
    return QStringLiteral("%1@%2x%3").arg(cacheKey).arg(requestedSize.width()).arg(requestedSize.height());
}

[[nodiscard]] QImage decode(const QByteArray& bytes, const QSize& requestedSize) {
    QBuffer buffer;
    buffer.setData(bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    reader.setAutoTransform(true);
    const QSize original = reader.size();
    if (requestedSize.isValid() && original.isValid() && !requestedSize.isEmpty()) {
        reader.setScaledSize(original.scaled(requestedSize, Qt::KeepAspectRatio));
    }
    return reader.read();
}

/// One request. The engine keeps it alive until finished() is emitted - also
/// after cancel() (Qt contract) - so callbacks may hold a raw pointer, and
/// finished() is emitted exactly once on every path.
class MediaImageResponse final : public QQuickImageResponse {
public:
    MediaImageResponse(MediaImageProvider& provider, MediaImageFetcher& fetcher, const QString& id,
                       const QSize& requestedSize)
        : provider_(provider) {
        const ImageRequest request = parseImageId(id);
        const QString key = memoryKey(request.cacheKey, requestedSize);
        if (QImage hit = provider_.cached(key); !hit.isNull()) {
            finish(std::move(hit), {});
            return;
        }
        if (!request.url.isValid() || request.url.isEmpty()) {
            finish({}, QStringLiteral("No image source"));
            return;
        }
        fetcher.fetch(request.url, [this, key, requestedSize](QByteArray bytes, QString error) {
            if (!error.isEmpty()) {
                finish({}, error);
                return;
            }
            QThreadPool::globalInstance()->start([this, key, requestedSize, bytes = std::move(bytes)]() {
                QImage image = decode(bytes, requestedSize);
                if (image.isNull()) {
                    finish({}, QStringLiteral("Cannot decode image"));
                    return;
                }
                provider_.store(key, image);
                finish(std::move(image), {});
            });
        });
    }

    [[nodiscard]] QQuickTextureFactory* textureFactory() const override {
        return QQuickTextureFactory::textureFactoryForImage(image_);
    }

    [[nodiscard]] QString errorString() const override { return error_; }

private:
    void finish(QImage image, QString error) {
        image_ = std::move(image);
        error_ = std::move(error);
        // Queued so a synchronous cache hit is not reported from inside the constructor.
        QMetaObject::invokeMethod(this, &QQuickImageResponse::finished, Qt::QueuedConnection);
    }

    MediaImageProvider& provider_;
    QImage image_;
    QString error_;
};

} // namespace

MediaImageFetcher::MediaImageFetcher(const QString& cacheDirectory, qint64 maxDiskCacheBytes, QObject* parent)
    : QObject(parent), network_(std::make_unique<QNetworkAccessManager>()) {
    auto* disk = new QNetworkDiskCache(network_.get()); // owned by the manager via setCache
    disk->setCacheDirectory(cacheDirectory);
    disk->setMaximumCacheSize(maxDiskCacheBytes);
    network_->setCache(disk);
    network_->setTransferTimeout(30'000);
}

MediaImageFetcher::~MediaImageFetcher() = default;

void MediaImageFetcher::fetch(const QUrl& url, std::function<void(QByteArray, QString)> done) {
    QMetaObject::invokeMethod(
        this,
        [this, url, done = std::move(done)]() {
            QNetworkRequest request(url);
            request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
            QNetworkReply* reply = network_->get(request);
            connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64) {
                if (received > kMaxImageBytes) {
                    reply->abort(); // never buffer an unbounded body into memory
                }
            });
            connect(reply, &QNetworkReply::finished, this, [reply, done]() {
                reply->deleteLater();
                if (reply->error() != QNetworkReply::NoError) {
                    done({}, reply->errorString());
                    return;
                }
                done(reply->readAll(), {});
            });
        },
        Qt::QueuedConnection);
}

MediaImageProvider::MediaImageProvider(MediaImageFetcher& fetcher, qsizetype memoryCacheKiB)
    : fetcher_(fetcher), memory_(memoryCacheKiB) {}

QQuickImageResponse* MediaImageProvider::requestImageResponse(const QString& id, const QSize& requestedSize) {
    return new MediaImageResponse(*this, fetcher_, id, requestedSize);
}

QImage MediaImageProvider::cached(const QString& key) const {
    const QMutexLocker lock(&mutex_);
    const QImage* image = memory_.object(key);
    return image != nullptr ? *image : QImage();
}

void MediaImageProvider::store(const QString& key, const QImage& image) {
    const QMutexLocker lock(&mutex_);
    memory_.insert(key, new QImage(image), static_cast<qsizetype>(std::max<qint64>(1, image.sizeInBytes() / 1024)));
}

} // namespace aedst::media
