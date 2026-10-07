#include "aedst/media/image_compressor.hpp"
#include "aedst/media/media_list_model.hpp"
#include "aedst/media/media_sources.hpp"

#include <QColor>
#include <QGuiApplication>
#include <QImage>
#include <QImageReader>
#include <QTemporaryDir>
#include <gtest/gtest.h>

namespace aedst::media {
namespace {

mediav1::Media readyImage() {
    mediav1::Media media;
    media.set_id("11111111-1111-4111-8111-111111111111");
    media.set_kind(mediav1::MEDIA_KIND_IMAGE);
    media.set_status(mediav1::MEDIA_STATUS_READY);
    media.set_url("https://storage.example/original.jpg?X-Amz-Signature=abc&x=1");
    auto* small = media.add_variants();
    small->set_kind(mediav1::VARIANT_KIND_THUMBNAIL);
    small->set_url("https://storage.example/thumb-128.jpg?sig=1");
    small->set_width(128);
    small->set_height(96);
    auto* large = media.add_variants();
    large->set_kind(mediav1::VARIANT_KIND_THUMBNAIL);
    large->set_url("https://storage.example/thumb-512.jpg?sig=2");
    large->set_width(512);
    large->set_height(384);
    auto* preview = media.add_variants();
    preview->set_kind(mediav1::VARIANT_KIND_PREVIEW);
    preview->set_url("https://storage.example/preview.jpg?sig=3");
    preview->set_width(1600);
    preview->set_height(1200);
    return media;
}

TEST(MediaSourcesTest, PicksTheSmallestVariantCoveringTheTarget) {
    const auto media = readyImage();
    EXPECT_EQ(selectVariant(media, DisplayPurpose::Thumbnail, 100)->width(), 128);
    EXPECT_EQ(selectVariant(media, DisplayPurpose::Thumbnail, 256)->width(), 512);
    // Nothing covers 4000 px: the largest of the preferred kind wins.
    EXPECT_EQ(selectVariant(media, DisplayPurpose::Thumbnail, 4000)->width(), 512);
    EXPECT_EQ(selectVariant(media, DisplayPurpose::Preview, 800)->kind(), mediav1::VARIANT_KIND_PREVIEW);
}

TEST(MediaSourcesTest, NotReadyMeansNothingToShow) {
    auto media = readyImage();
    media.set_status(mediav1::MEDIA_STATUS_PROCESSING);
    EXPECT_EQ(selectVariant(media, DisplayPurpose::Thumbnail, 128), nullptr);
    EXPECT_TRUE(imageSourceFor(media, DisplayPurpose::Thumbnail, 128).isEmpty());
}

TEST(MediaSourcesTest, ImageIdRoundTripsThePresignedUrl) {
    const auto media = readyImage();
    const QString source = imageSourceFor(media, DisplayPurpose::Thumbnail, 100);
    ASSERT_TRUE(source.startsWith(QStringLiteral("image://media/")));
    const ImageRequest request = parseImageId(source.mid(QStringLiteral("image://media/").size()));
    EXPECT_EQ(request.cacheKey, QStringLiteral("11111111-1111-4111-8111-111111111111/thumbnail"));
    EXPECT_EQ(request.url, QUrl(QStringLiteral("https://storage.example/thumb-128.jpg?sig=1")));
}

TEST(MediaSourcesTest, ImageIdNeverResolvesToLocalFiles) {
    const QString encoded = QString::fromLatin1(QByteArray("file:///etc/passwd").toPercentEncoding());
    EXPECT_TRUE(parseImageId(QStringLiteral("x/original?u=") + encoded).url.isEmpty());
}

TEST(ImageCompressorTest, FitWithinKeepsAspectAndNeverUpscales) {
    EXPECT_EQ(fitWithin(QSize(4000, 3000), 2000), QSize(2000, 1500));
    EXPECT_EQ(fitWithin(QSize(1000, 3000), 1500), QSize(500, 1500));
    EXPECT_EQ(fitWithin(QSize(800, 600), 2000), QSize(800, 600));
}

TEST(ImageCompressorTest, DownscalesLargePhotosToJpeg) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QImage photo(3000, 2000, QImage::Format_RGB32);
    photo.fill(QColor(30, 120, 200));
    const QString input = dir.filePath(QStringLiteral("in.png"));
    ASSERT_TRUE(photo.save(input, "PNG"));

    const auto result = compressImage(input, dir.filePath(QStringLiteral("out.jpg")),
                                      CompressionOptions{.maxDimension = 1000, .quality = 70});
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->compressed);
    EXPECT_EQ(result->contentType, QByteArray("image/jpeg"));
    EXPECT_EQ(QImageReader(result->path).size(), QSize(1000, 667));
}

TEST(ImageCompressorTest, KeepsSmallImagesUntouched) {
    QTemporaryDir dir;
    QImage small(64, 64, QImage::Format_RGB32);
    small.fill(Qt::white);
    const QString input = dir.filePath(QStringLiteral("small.png"));
    ASSERT_TRUE(small.save(input, "PNG"));
    const auto result = compressImage(input, dir.filePath(QStringLiteral("out.jpg")));
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->compressed);
    EXPECT_EQ(result->path, input);
}

TEST(MediaListModelTest, UpsertReplacesByIdAndTracksUnfinished) {
    MediaListModel model;
    auto media = readyImage();
    model.setItems({media});
    EXPECT_EQ(model.count(), 1);
    EXPECT_FALSE(model.hasUnfinished());

    media.set_status(mediav1::MEDIA_STATUS_PROCESSING);
    model.upsert(media);
    EXPECT_EQ(model.count(), 1);
    EXPECT_TRUE(model.hasUnfinished());
    EXPECT_EQ(model.data(model.index(0), MediaListModel::StatusRole).toString(), QStringLiteral("processing"));

    model.remove(QString::fromStdString(media.id()));
    EXPECT_EQ(model.count(), 0);
}

} // namespace
} // namespace aedst::media

// QImage encoders/decoders need a QGuiApplication (offscreen in CI).
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    QGuiApplication app(argc, argv);
    return RUN_ALL_TESTS();
}
