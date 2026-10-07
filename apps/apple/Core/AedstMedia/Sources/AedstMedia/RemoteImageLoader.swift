import AedstCore
import CoreGraphics
import Foundation

/// Loads and caches remote images.
///
/// Why not `AsyncImage` alone: media URLs from `MediaService` are short-lived
/// *presigned* URLs, so the URL (and therefore `URLCache`'s key) changes on
/// every `GetMedia`/`ListMedia` call and `AsyncImage`'s shared-session cache
/// would never hit. This loader stores responses in a dedicated `URLCache`
/// (platform API, on-disk + in-memory) under a **stable key** — the media id
/// plus variant — and keeps decoded, downsampled bitmaps in an `NSCache`.
/// Without a `cacheKey` it still works, keyed by the URL.
public actor RemoteImageLoader {
    /// Process-wide loader (one disk cache directory).
    public static let shared = RemoteImageLoader()

    private let session: URLSession
    private let cache: URLCache
    private let decoded = NSCache<NSString, CGImageBox>()

    /// `memoryCapacity`/`diskCapacity` in bytes.
    public init(memoryCapacity: Int = 32 * 1_024 * 1_024, diskCapacity: Int = 256 * 1_024 * 1_024) {
        let directory = FileManager.default.urls(for: .cachesDirectory, in: .userDomainMask).first?
            .appendingPathComponent("AEDSTMediaCache", isDirectory: true)
        cache = URLCache(memoryCapacity: memoryCapacity, diskCapacity: diskCapacity, directory: directory)

        let configuration = URLSessionConfiguration.default
        // We write the cache ourselves under a stable key; never cache by presigned URL.
        configuration.urlCache = nil
        configuration.requestCachePolicy = .reloadIgnoringLocalCacheData
        configuration.httpCookieStorage = nil
        configuration.timeoutIntervalForRequest = 30
        session = URLSession(configuration: configuration)
        decoded.countLimit = 200
    }

    /// Returns the image at `url`, downsampled to `maxPixelSize`, from cache when possible.
    public func image(url: URL, cacheKey: String?, maxPixelSize: Int) async throws -> DecodedImage {
        let memoryKey = "\(cacheKey ?? url.absoluteString)@\(maxPixelSize)" as NSString
        if let box = decoded.object(forKey: memoryKey) {
            return DecodedImage(box.image)
        }

        let data = try await data(url: url, cacheKey: cacheKey)
        guard let image = ImageDownsampler.downsample(data: data, maxPixelSize: maxPixelSize) else {
            throw AedstError.decoding(reason: "Undecodable image data")
        }
        decoded.setObject(CGImageBox(image.cgImage), forKey: memoryKey)
        return image
    }

    /// Drops every cached response and bitmap (e.g. on sign-out).
    public func removeAll() {
        cache.removeAllCachedResponses()
        decoded.removeAllObjects()
    }

    private func data(url: URL, cacheKey: String?) async throws -> Data {
        let keyRequest = URLRequest(url: cacheKey.flatMap(Self.stableURL(for:)) ?? url)
        if let cached = cache.cachedResponse(for: keyRequest) {
            return cached.data
        }

        let (data, response) = try await session.data(for: URLRequest(url: url))
        guard let http = response as? HTTPURLResponse, (200..<300).contains(http.statusCode) else {
            throw AedstError.network(reason: "Image request failed")
        }
        if let keyURL = keyRequest.url,
           let stored = HTTPURLResponse(
               url: keyURL,
               statusCode: 200,
               httpVersion: "HTTP/1.1",
               headerFields: ["Content-Type": http.mimeType ?? "application/octet-stream"]
           ) {
            cache.storeCachedResponse(CachedURLResponse(response: stored, data: data, storagePolicy: .allowed), for: keyRequest)
        }
        return data
    }

    /// A synthetic, never-requested URL used only as the cache key.
    static func stableURL(for key: String) -> URL? {
        var components = URLComponents()
        components.scheme = "https"
        components.host = "media-cache.aedst.invalid"
        components.path = "/" + key
        return components.url
    }
}

/// `NSCache` values must be class instances.
final class CGImageBox {
    let image: CGImage

    init(_ image: CGImage) {
        self.image = image
    }
}
