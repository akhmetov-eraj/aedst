#include "aedst/media/video_frame_grabber.hpp"

#include "aedst/media/image_compressor.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QMediaPlayer>
#include <QStandardPaths>
#include <QVideoFrame>
#include <QVideoSink>

#include <algorithm>
#include <utility>

namespace aedst::media {

VideoFrameGrabber::VideoFrameGrabber(QObject* parent) : QObject(parent) {
    timeout_.setSingleShot(true);
    connect(&timeout_, &QTimer::timeout, this, [this]() { finish({}); });
}

VideoFrameGrabber::~VideoFrameGrabber() = default;

void VideoFrameGrabber::setSource(const QUrl& source) {
    if (source == source_) {
        return;
    }
    source_ = source;
    emit sourceChanged();
    restart();
}

void VideoFrameGrabber::setPositionMs(qint64 positionMs) {
    if (positionMs == positionMs_) {
        return;
    }
    positionMs_ = std::max<qint64>(0, positionMs);
    emit sourceChanged();
    restart();
}

void VideoFrameGrabber::setMaxPixels(int maxPixels) {
    if (maxPixels == maxPixels_ || maxPixels <= 0) {
        return;
    }
    maxPixels_ = maxPixels;
    emit sourceChanged();
    restart();
}

void VideoFrameGrabber::restart() {
    timeout_.stop();
    player_.reset();
    sink_.reset();
    thumbnailUrl_.clear();
    image_ = {};
    failed_ = false;
    seeked_ = false;
    emit resultChanged();
    if (source_.isEmpty()) {
        return;
    }

    player_ = std::make_unique<QMediaPlayer>();
    sink_ = std::make_unique<QVideoSink>();
    // No QAudioOutput is attached, so nothing is ever audible.
    player_->setVideoSink(sink_.get());
    connect(sink_.get(), &QVideoSink::videoFrameChanged, this, &VideoFrameGrabber::onFrame);
    connect(player_.get(), &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::LoadedMedia && !seeked_) {
            seeked_ = true;
            const qint64 duration = player_->duration();
            player_->setPosition(duration > 0 ? std::min(positionMs_, duration / 2) : positionMs_);
            player_->play();
        } else if (status == QMediaPlayer::InvalidMedia) {
            finish({});
        }
    });
    connect(player_.get(), &QMediaPlayer::errorOccurred, this, [this]() { finish({}); });
    player_->setSource(source_);
    timeout_.start(kTimeoutMs);
}

void VideoFrameGrabber::onFrame(const QVideoFrame& frame) {
    if (!frame.isValid() || !player_ || player_->position() + 50 < std::min(positionMs_, player_->duration() / 2)) {
        return; // decoders may deliver a frame from before the seek target first
    }
    QImage image = frame.toImage();
    if (image.isNull()) {
        return;
    }
    const QSize target = fitWithin(image.size(), maxPixels_);
    if (target != image.size()) {
        image = image.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    finish(std::move(image));
}

void VideoFrameGrabber::finish(QImage image) {
    timeout_.stop();
    if (player_) {
        player_->stop();
        // Deleting the player from inside its own signal is unsafe; defer it.
        player_.release()->deleteLater();
        sink_.release()->deleteLater();
    }
    if (image.isNull()) {
        failed_ = true;
        emit resultChanged();
        return;
    }
    image_ = std::move(image);
    const QString dir = QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
                            .filePath(QStringLiteral("video-thumbnails"));
    QDir().mkpath(dir);
    const QByteArray name =
        QCryptographicHash::hash(source_.toEncoded() + QByteArray::number(positionMs_), QCryptographicHash::Sha256)
            .toHex();
    const QString path = QDir(dir).filePath(QString::fromLatin1(name) + QStringLiteral(".png"));
    if (image_.save(path, "PNG")) {
        thumbnailUrl_ = QUrl::fromLocalFile(path);
    } else {
        failed_ = true;
    }
    emit resultChanged();
}

} // namespace aedst::media
