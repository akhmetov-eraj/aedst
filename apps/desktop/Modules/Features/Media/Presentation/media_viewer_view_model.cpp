#include "Presentation/media_viewer_view_model.hpp"

#include "aedst/core/error_sink.hpp"
#include "aedst/media/media_remote_data_source.hpp"
#include "aedst/media/media_sources.hpp"

#include <QPointer>

#include <algorithm>
#include <utility>

namespace aedst::feature::media {

namespace mediav1 = ::aedst::media::v1;
using aedst::media::DisplayPurpose;

namespace {

constexpr int kPreviewPixels = 2048;
constexpr int kPollMs = 3000;
constexpr int kMaxPollAttempts = 40;

} // namespace

MediaViewerViewModel::MediaViewerViewModel(std::shared_ptr<aedst::media::MediaRemoteDataSource> remote,
                                           std::shared_ptr<core::ErrorSink> errors, QObject* parent)
    : QObject(parent), remote_(std::move(remote)), errors_(std::move(errors)) {
    poll_.setSingleShot(true);
    poll_.setInterval(kPollMs);
    connect(&poll_, &QTimer::timeout, this, &MediaViewerViewModel::refresh);
}

QString MediaViewerViewModel::kind() const {
    return media_ ? aedst::media::kindName(media_->kind()) : QString();
}

QString MediaViewerViewModel::status() const {
    return media_ ? aedst::media::statusName(media_->status()) : QString();
}

QString MediaViewerViewModel::fileName() const {
    return media_ ? QString::fromStdString(media_->filename()) : QString();
}

QString MediaViewerViewModel::failureReason() const {
    return media_ ? QString::fromStdString(media_->failure_reason()) : QString();
}

QString MediaViewerViewModel::previewSource() const {
    return media_ ? aedst::media::imageSourceFor(*media_, DisplayPurpose::Preview, kPreviewPixels) : QString();
}

QUrl MediaViewerViewModel::playbackUrl() const {
    if (!media_ || (media_->kind() != mediav1::MEDIA_KIND_VIDEO && media_->kind() != mediav1::MEDIA_KIND_AUDIO)) {
        return {};
    }
    return aedst::media::urlFor(*media_, DisplayPurpose::Playback, kPreviewPixels);
}

QString MediaViewerViewModel::posterSource() const {
    return media_ && media_->kind() == mediav1::MEDIA_KIND_VIDEO
               ? aedst::media::imageSourceFor(*media_, DisplayPurpose::Thumbnail, kPreviewPixels)
               : QString();
}

qint64 MediaViewerViewModel::sizeBytes() const {
    return media_ ? static_cast<qint64>(media_->size_bytes()) : 0;
}

void MediaViewerViewModel::load(const QString& mediaId) {
    if (mediaId != requestedId_) {
        media_.reset();
        emit mediaChanged();
    }
    requestedId_ = mediaId;
    pollAttempts_ = 0;
    refresh();
}

void MediaViewerViewModel::refresh() {
    if (requestedId_.isEmpty()) {
        return;
    }
    poll_.stop();
    setErrorMessage({});
    setLoading(true);
    mediav1::GetMediaRequest request;
    request.set_id(requestedId_.toStdString());
    const quint64 seq = ++seq_;
    QPointer<MediaViewerViewModel> guard(this);
    remote_->getMedia(request, [guard, seq](network::ApiResult<mediav1::GetMediaResponse> result) {
        if (!guard || seq != guard->seq_) {
            return;
        }
        guard->setLoading(false);
        if (!result) {
            guard->setErrorMessage(QString::fromStdString(result.error().message));
            if (guard->errors_) {
                guard->errors_->report(result.error().toCoreError(), "media.viewer", core::ErrorVisibility::Silent);
            }
            return;
        }
        guard->media_ = result->media();
        emit guard->mediaChanged();
        const auto status = result->media().status();
        if ((status == mediav1::MEDIA_STATUS_PENDING_UPLOAD || status == mediav1::MEDIA_STATUS_PROCESSING) &&
            guard->pollAttempts_ < kMaxPollAttempts) {
            ++guard->pollAttempts_;
            guard->poll_.start();
        }
    });
}

void MediaViewerViewModel::clear() {
    ++seq_;
    poll_.stop();
    requestedId_.clear();
    media_.reset();
    emit mediaChanged();
    setErrorMessage({});
    setLoading(false);
}

void MediaViewerViewModel::setLoading(bool loading) {
    if (loading != isLoading_) {
        isLoading_ = loading;
        emit isLoadingChanged();
    }
}

void MediaViewerViewModel::setErrorMessage(const QString& message) {
    if (message != errorMessage_) {
        errorMessage_ = message;
        emit errorMessageChanged();
    }
}

} // namespace aedst::feature::media
