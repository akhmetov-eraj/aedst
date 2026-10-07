import AedstDesignSystem
import SwiftUI

/// Remote image with a skeleton while loading, a placeholder on failure, and
/// caching/downsampling through `RemoteImageLoader`. Pass a `cacheKey` (media
/// id + variant) whenever the URL is presigned and therefore changes per call.
public struct AedstRemoteImage: View {
    private let url: URL?
    private let cacheKey: String?
    private let maxPointSize: CGFloat
    private let contentMode: ContentMode
    private let accessibilityLabel: String?
    private let loader: RemoteImageLoader

    @State private var image: DecodedImage?
    @State private var failed = false
    @Environment(\.displayScale) private var displayScale

    /// `maxPointSize` is the largest side the image is shown at, in points.
    public init(
        url: URL?,
        cacheKey: String? = nil,
        maxPointSize: CGFloat = 600,
        contentMode: ContentMode = .fill,
        accessibilityLabel: String? = nil,
        loader: RemoteImageLoader = .shared
    ) {
        self.url = url
        self.cacheKey = cacheKey
        self.maxPointSize = maxPointSize
        self.contentMode = contentMode
        self.accessibilityLabel = accessibilityLabel
        self.loader = loader
    }

    public var body: some View {
        ZStack {
            if let image {
                Image(decorative: image.cgImage, scale: displayScale)
                    .resizable()
                    .aspectRatio(contentMode: contentMode)
                    .accessibilityHidden(accessibilityLabel == nil)
                    .accessibilityLabel(Text(accessibilityLabel ?? ""))
            } else if failed || url == nil {
                Image(systemName: "photo")
                    .foregroundStyle(AedstColor.mutedForeground)
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
                    .background(AedstColor.muted)
                    .accessibilityLabel(Text(AedstCommonStrings.imageUnavailable))
            } else {
                AedstSkeleton()
            }
        }
        .clipped()
        .task(id: url) { await load() }
    }

    private func load() async {
        guard let url else { return }
        failed = false
        let pixels = Int((maxPointSize * displayScale).rounded(.up))
        do {
            image = try await loader.image(url: url, cacheKey: cacheKey, maxPixelSize: pixels)
        } catch {
            if !Task.isCancelled { failed = true }
        }
    }
}
