#include "aedst/media/image_compressor.hpp"

#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QMimeDatabase>

#include <algorithm>
#include <cmath>

namespace aedst::media {
namespace {

[[nodiscard]] CompressedImage passThrough(const QString& path, QSize size = {}) {
    const QFileInfo info(path);
    return CompressedImage{
        .path = path,
        .contentType = QMimeDatabase().mimeTypeForFile(info).name().toUtf8(),
        .sizeBytes = info.size(),
        .size = size,
        .compressed = false,
    };
}

} // namespace

QSize fitWithin(QSize original, int maxDimension) {
    if (!original.isValid() || maxDimension <= 0) {
        return original;
    }
    const int longer = std::max(original.width(), original.height());
    if (longer <= maxDimension) {
        return original;
    }
    const double scale = static_cast<double>(maxDimension) / static_cast<double>(longer);
    return QSize(std::max(1, static_cast<int>(std::lround(original.width() * scale))),
                 std::max(1, static_cast<int>(std::lround(original.height() * scale))));
}

QImage readScaledImage(const QString& path, int maxPixels) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QSize target = fitWithin(reader.size(), maxPixels);
    if (target.isValid()) {
        reader.setScaledSize(target);
    }
    return reader.read();
}

std::expected<CompressedImage, QString> compressImage(const QString& inputPath, const QString& outputPath,
                                                      const CompressionOptions& options) {
    const QFileInfo input(inputPath);
    if (!input.isFile() || !input.isReadable()) {
        return std::unexpected(QStringLiteral("File is not readable"));
    }

    QImageReader reader(inputPath);
    reader.setAutoTransform(true);
    if (!reader.canRead()) {
        return passThrough(inputPath); // not an image Qt can decode: upload as-is
    }
    if (reader.supportsAnimation() && reader.imageCount() > 1) {
        return passThrough(inputPath, reader.size()); // keep animations intact
    }

    const QSize original = reader.size();
    const QSize target = fitWithin(original, options.maxDimension);
    if (input.size() < options.skipBelowBytes && target == original) {
        return passThrough(inputPath, original);
    }
    if (target.isValid() && target != original) {
        reader.setScaledSize(target);
    }
    QImage image = reader.read();
    if (image.isNull()) {
        return passThrough(inputPath, original);
    }

    // Keep transparency lossless; everything else becomes the configured lossy format.
    const bool keepPng = image.hasAlphaChannel();
    const QByteArray format = keepPng ? QByteArrayLiteral("png") : options.format;

    QImageWriter writer(outputPath, format);
    if (!keepPng) {
        writer.setQuality(std::clamp(options.quality, 0, 100));
    }
    writer.setOptimizedWrite(true);
    writer.setProgressiveScanWrite(true);
    if (!writer.write(image)) {
        return passThrough(inputPath, original);
    }

    const QFileInfo output(outputPath);
    // Re-encoding can make an already-optimized small image bigger: keep the smaller file.
    if (output.size() >= input.size() && target == original) {
        QFile::remove(outputPath);
        return passThrough(inputPath, original);
    }
    return CompressedImage{
        .path = outputPath,
        .contentType = QByteArrayLiteral("image/") + format,
        .sizeBytes = output.size(),
        .size = image.size(),
        .compressed = true,
    };
}

} // namespace aedst::media
