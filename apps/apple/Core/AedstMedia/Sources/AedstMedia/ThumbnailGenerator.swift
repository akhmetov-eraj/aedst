import AVFoundation
import CoreGraphics
import Foundation
import QuickLookThumbnailing

/// Client-side thumbnails — the **fallback** path. The primary source is the
/// server's `VARIANT_KIND_THUMBNAIL` / `VARIANT_KIND_POSTER` variants (made by
/// the media module's processing job); these are used while a file is still
/// uploading/processing, or when the server produced no variant.
public enum ThumbnailGenerator {
    /// Thumbnail of a **local** file (image, video, PDF, ...) via QuickLookThumbnailing.
    public static func thumbnail(forFileAt fileURL: URL, size: CGSize, scale: CGFloat) async -> DecodedImage? {
        let request = QLThumbnailGenerator.Request(
            fileAt: fileURL,
            size: size,
            scale: scale,
            representationTypes: .thumbnail
        )
        do {
            let representation = try await QLThumbnailGenerator.shared.generateBestRepresentation(for: request)
            return DecodedImage(representation.cgImage)
        } catch {
            return nil
        }
    }

    /// Poster frame of a (local or remote) video via `AVAssetImageGenerator`.
    public static func posterFrame(forVideoAt url: URL, maximumPixelSize: CGSize) async -> DecodedImage? {
        let generator = AVAssetImageGenerator(asset: AVURLAsset(url: url))
        generator.appliesPreferredTrackTransform = true
        generator.maximumSize = maximumPixelSize
        // Skip the first (often black) frames.
        let time = CMTime(seconds: 0.5, preferredTimescale: 600)

        if #available(iOS 16, macOS 13, *) {
            do {
                let (image, _) = try await generator.image(at: time)
                return DecodedImage(image)
            } catch {
                return nil
            }
        } else {
            // iOS 15 / macOS 12: the async API does not exist; the synchronous one
            // blocks this (non-main) task's thread for the duration of the decode.
            guard let image = try? generator.copyCGImage(at: time, actualTime: nil) else { return nil }
            return DecodedImage(image)
        }
    }
}
