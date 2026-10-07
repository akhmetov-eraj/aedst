#include "Presentation/media_library_view_model.hpp"

#include "Navigation/media_destinations.hpp"
#include "aedst/analytics/analytics_tracker.hpp"
#include "aedst/core/error_sink.hpp"
#include "aedst/media/media_remote_data_source.hpp"
#include "aedst/navigation/router.hpp"

#include <QPointer>

#include <algorithm>
#include <utility>

namespace aedst::feature::media {

namespace mediav1 = ::aedst::media::v1;

namespace {

constexpr int kFirstPollMs = 2000;
constexpr int kMaxPollMs = 30'000;
constexpr int kMaxPollAttempts = 20;

[[nodiscard]] mediav1::MediaKind kindFromFilter(const QString& filter) {
    if (filter == QStringLiteral("image")) {
        return mediav1::MEDIA_KIND_IMAGE;
    }
    if (filter == QStringLiteral("video")) {
        return mediav1::MEDIA_KIND_VIDEO;
    }
    if (filter == QStringLiteral("document")) {
        return mediav1::MEDIA_KIND_DOCUMENT;
    }
    return mediav1::MEDIA_KIND_UNSPECIFIED;
}

} // namespace

MediaLibraryViewModel::MediaLibraryViewModel(Dependencies dependencies, QObject* parent)
    : QObject(parent), deps_(std::move(dependencies)), uploader_(deps_.upload, this) {
    poll_.setSingleShot(true);
    connect(&poll_, &QTimer::timeout, this, [this]() { load(false); });
    connect(&uploader_, &aedst::media::MediaUploadController::uploaded, this, [this](const QString&) {
        if (deps_.analytics) {
            deps_.analytics->track(analytics::events::kMediaUploaded, {});
        }
        pollAttempts_ = 0;
        load(false);
    });
}

void MediaLibraryViewModel::setKindFilter(const QString& kind) {
    if (kind == kindFilter_) {
        return;
    }
    kindFilter_ = kind;
    emit kindFilterChanged();
    reload();
}

void MediaLibraryViewModel::reload() {
    pollAttempts_ = 0;
    load(false);
}

void MediaLibraryViewModel::loadMore() {
    if (hasMore() && !isLoading_) {
        load(true);
    }
}

void MediaLibraryViewModel::load(bool append) {
    poll_.stop();
    setErrorMessage({});
    setLoading(true);
    mediav1::ListMediaRequest request;
    request.set_page_size(60);
    if (append) {
        request.set_page_token(nextPageToken_);
    }
    if (const auto kind = kindFromFilter(kindFilter_); kind != mediav1::MEDIA_KIND_UNSPECIFIED) {
        request.add_kinds(kind);
    }
    const quint64 seq = ++seq_;
    QPointer<MediaLibraryViewModel> guard(this);
    deps_.remote->listMedia(request, [guard, seq, append](network::ApiResult<mediav1::ListMediaResponse> result) {
        if (!guard || seq != guard->seq_) {
            return;
        }
        guard->setLoading(false);
        if (!result) {
            guard->setErrorMessage(QString::fromStdString(result.error().message));
            if (guard->deps_.errors) {
                guard->deps_.errors->report(result.error().toCoreError(), "media.library",
                                            append ? core::ErrorVisibility::Transient : core::ErrorVisibility::Silent);
            }
            return;
        }
        std::vector<mediav1::Media> items(result->media().begin(), result->media().end());
        if (append) {
            guard->items_.append(std::move(items));
        } else {
            guard->items_.setItems(std::move(items));
        }
        guard->setNextPageToken(result->next_page_token());
        if (guard->items_.hasUnfinished()) {
            guard->schedulePoll(); // thumbnails appear once server-side processing is done
        }
    });
}

void MediaLibraryViewModel::schedulePoll() {
    if (pollAttempts_ >= kMaxPollAttempts) {
        return;
    }
    poll_.start(std::min(kMaxPollMs, kFirstPollMs * (1 << std::min(pollAttempts_, 4))));
    ++pollAttempts_;
}

void MediaLibraryViewModel::open(const QString& mediaId) {
    if (deps_.router != nullptr && !mediaId.isEmpty()) {
        deps_.router->push(mediaViewerRoute(mediaId.toStdString()));
    }
}

void MediaLibraryViewModel::remove(const QString& mediaId) {
    mediav1::DeleteMediaRequest request;
    request.set_id(mediaId.toStdString());
    QPointer<MediaLibraryViewModel> guard(this);
    deps_.remote->deleteMedia(request, [guard, mediaId](network::ApiResult<void> result) {
        if (!guard) {
            return;
        }
        if (!result) {
            if (guard->deps_.errors) {
                guard->deps_.errors->report(result.error().toCoreError(), "media.delete");
            }
            return;
        }
        guard->items_.remove(mediaId);
    });
}

void MediaLibraryViewModel::clear() {
    ++seq_;
    poll_.stop();
    uploader_.abortAll();
    items_.setItems({});
    setNextPageToken({});
    setErrorMessage({});
    setLoading(false);
}

void MediaLibraryViewModel::setLoading(bool loading) {
    if (loading != isLoading_) {
        isLoading_ = loading;
        emit isLoadingChanged();
    }
}

void MediaLibraryViewModel::setErrorMessage(const QString& message) {
    if (message != errorMessage_) {
        errorMessage_ = message;
        emit errorMessageChanged();
    }
}

void MediaLibraryViewModel::setNextPageToken(std::string token) {
    const bool before = hasMore();
    nextPageToken_ = std::move(token);
    if (before != hasMore()) {
        emit hasMoreChanged();
    }
}

} // namespace aedst::feature::media
