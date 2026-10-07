#include "aedst/media/media_list_model.hpp"

#include "aedst/media/media_sources.hpp"

#include <QUrl>

#include <algorithm>
#include <iterator>
#include <utility>

namespace aedst::media {
namespace {

constexpr int kThumbnailPixels = 256;
constexpr int kPreviewPixels = 1600;

} // namespace

MediaListModel::MediaListModel(QObject* parent) : QAbstractListModel(parent) {}

int MediaListModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(items_.size());
}

QVariant MediaListModel::data(const QModelIndex& index, int role) const {
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return {};
    }
    const mediav1::Media& media = items_.at(static_cast<std::size_t>(index.row()));
    switch (role) {
    case Qt::DisplayRole:
    case FileNameRole:
        return QString::fromStdString(media.filename());
    case MediaIdRole:
        return QString::fromStdString(media.id());
    case KindRole:
        return kindName(media.kind());
    case StatusRole:
        return statusName(media.status());
    case ContentTypeRole:
        return QString::fromStdString(media.content_type());
    case SizeBytesRole:
        return static_cast<qlonglong>(media.size_bytes());
    case WidthRole:
        return media.width();
    case HeightRole:
        return media.height();
    case DurationMsRole:
        return static_cast<qlonglong>(media.duration_ms());
    case ThumbnailSourceRole:
        return imageSourceFor(media, DisplayPurpose::Thumbnail, kThumbnailPixels);
    case PreviewSourceRole:
        return imageSourceFor(media, DisplayPurpose::Preview, kPreviewPixels);
    case PlaybackUrlRole:
        return media.kind() == mediav1::MEDIA_KIND_VIDEO || media.kind() == mediav1::MEDIA_KIND_AUDIO
                   ? urlFor(media, DisplayPurpose::Playback, kPreviewPixels)
                   : QUrl();
    case OriginalUrlRole:
        return QUrl(QString::fromStdString(media.url()));
    case FailureReasonRole:
        return QString::fromStdString(media.failure_reason());
    case TaskIdRole:
        return QString::fromStdString(media.task_id());
    default:
        break;
    }
    return {};
}

QHash<int, QByteArray> MediaListModel::roleNames() const {
    return {
        {MediaIdRole, "mediaId"},
        {KindRole, "kind"},
        {StatusRole, "status"},
        {FileNameRole, "fileName"},
        {ContentTypeRole, "contentType"},
        {SizeBytesRole, "sizeBytes"},
        {WidthRole, "mediaWidth"},
        {HeightRole, "mediaHeight"},
        {DurationMsRole, "durationMs"},
        {ThumbnailSourceRole, "thumbnailSource"},
        {PreviewSourceRole, "previewSource"},
        {PlaybackUrlRole, "playbackUrl"},
        {OriginalUrlRole, "originalUrl"},
        {FailureReasonRole, "failureReason"},
        {TaskIdRole, "taskId"},
    };
}

void MediaListModel::setItems(std::vector<mediav1::Media> items) {
    const bool countChanges = items.size() != items_.size();
    beginResetModel();
    items_ = std::move(items);
    endResetModel();
    if (countChanges) {
        emit countChanged();
    }
}

void MediaListModel::append(std::vector<mediav1::Media> items) {
    if (items.empty()) {
        return;
    }
    const int first = static_cast<int>(items_.size());
    beginInsertRows({}, first, first + static_cast<int>(items.size()) - 1);
    std::ranges::move(items, std::back_inserter(items_));
    endInsertRows();
    emit countChanged();
}

void MediaListModel::upsert(const mediav1::Media& item) {
    const int row = indexOf(QString::fromStdString(item.id()));
    if (row >= 0) {
        items_[static_cast<std::size_t>(row)] = item;
        emit dataChanged(index(row), index(row));
        return;
    }
    const int first = static_cast<int>(items_.size());
    beginInsertRows({}, first, first);
    items_.push_back(item);
    endInsertRows();
    emit countChanged();
}

void MediaListModel::remove(const QString& mediaId) {
    const int row = indexOf(mediaId);
    if (row < 0) {
        return;
    }
    beginRemoveRows({}, row, row);
    items_.erase(items_.begin() + row);
    endRemoveRows();
    emit countChanged();
}

const mediav1::Media* MediaListModel::find(const QString& mediaId) const {
    const int row = indexOf(mediaId);
    return row < 0 ? nullptr : &items_.at(static_cast<std::size_t>(row));
}

bool MediaListModel::hasUnfinished() const {
    return std::ranges::any_of(items_, [](const mediav1::Media& media) {
        return media.status() == mediav1::MEDIA_STATUS_PENDING_UPLOAD ||
               media.status() == mediav1::MEDIA_STATUS_PROCESSING;
    });
}

int MediaListModel::indexOf(const QString& mediaId) const {
    const std::string id = mediaId.toStdString();
    const auto it = std::ranges::find_if(items_, [&id](const mediav1::Media& media) { return media.id() == id; });
    return it == items_.end() ? -1 : static_cast<int>(std::distance(items_.begin(), it));
}

} // namespace aedst::media
