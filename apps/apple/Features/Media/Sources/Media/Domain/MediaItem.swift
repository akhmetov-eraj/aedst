import AedstCore
import Foundation

/// An uploaded file (`aedst.media.v1.Media`).
public struct MediaItem: Sendable, Equatable, Identifiable {
    /// Mirrors `MediaKind`.
    public enum Kind: Sendable, Equatable {
        case image
        case video
        case audio
        case document
        case other
    }

    /// Mirrors `MediaStatus`.
    public enum Status: Sendable, Equatable {
        case pendingUpload
        case processing
        case ready
        case failed
        case unknown
    }

    public let id: String
    public let kind: Kind
    public let status: Status
    public let filename: String
    public let contentType: String
    public let sizeBytes: Int64
    public let durationMs: Int64
    /// Short-lived presigned GET URL of the original (`nil` until ready).
    public let url: URL?
    public let variants: [MediaVariant]
    public let taskID: String?
    public let createdAt: Date?
    public let failureReason: String?

    /// Memberwise initializer.
    public init(
        id: String,
        kind: Kind,
        status: Status,
        filename: String,
        contentType: String = "",
        sizeBytes: Int64 = 0,
        durationMs: Int64 = 0,
        url: URL? = nil,
        variants: [MediaVariant] = [],
        taskID: String? = nil,
        createdAt: Date? = nil,
        failureReason: String? = nil
    ) {
        self.id = id
        self.kind = kind
        self.status = status
        self.filename = filename
        self.contentType = contentType
        self.sizeBytes = sizeBytes
        self.durationMs = durationMs
        self.url = url
        self.variants = variants
        self.taskID = taskID
        self.createdAt = createdAt
        self.failureReason = failureReason
    }

    /// The first variant of `kind`, if the server produced one.
    public func variant(_ kind: MediaVariant.Kind) -> MediaVariant? {
        variants.first { $0.kind == kind }
    }

    /// Best URL to *play* a video: the optimized rendition, else the original.
    public var playbackURL: URL? {
        variant(.optimized)?.url ?? url
    }
}

/// A server-side rendition (thumbnail, preview, optimized, poster).
public struct MediaVariant: Sendable, Equatable {
    /// Mirrors `VariantKind`.
    public enum Kind: String, Sendable, Equatable {
        case thumbnail
        case preview
        case optimized
        case poster
        case other
    }

    public let kind: Kind
    /// Short-lived presigned GET URL.
    public let url: URL
    public let width: Int
    public let height: Int

    /// Memberwise initializer.
    public init(kind: Kind, url: URL, width: Int = 0, height: Int = 0) {
        self.kind = kind
        self.url = url
        self.width = width
        self.height = height
    }
}

/// `CreateUploadResponse`: where and how to PUT the bytes.
public struct UploadTicket: Sendable, Equatable {
    public let media: MediaItem
    public let uploadURL: URL
    public let method: String
    /// Headers to send verbatim with the upload.
    public let headers: [String: String]

    /// Memberwise initializer.
    public init(media: MediaItem, uploadURL: URL, method: String, headers: [String: String]) {
        self.media = media
        self.uploadURL = uploadURL
        self.method = method
        self.headers = headers
    }
}

/// Input of `CreateUpload`.
public struct UploadRequest: Sendable, Equatable {
    public let filename: String
    public let contentType: String
    public let sizeBytes: Int64
    public let taskID: String?
    /// Hex SHA-256 (object storage verifies it).
    public let sha256: String?

    /// Memberwise initializer.
    public init(filename: String, contentType: String, sizeBytes: Int64, taskID: String?, sha256: String?) {
        self.filename = filename
        self.contentType = contentType
        self.sizeBytes = sizeBytes
        self.taskID = taskID
        self.sha256 = sha256
    }
}

/// The seam to `MediaService`.
public protocol MediaRepository: Sendable {
    func createUpload(_ request: UploadRequest) async -> Outcome<UploadTicket>
    func completeUpload(id: String) async -> Outcome<MediaItem>
    func get(id: String) async -> Outcome<MediaItem>
    /// `taskID` filters to one task's attachments.
    func list(taskID: String?, pageToken: String?) async -> Outcome<Page<MediaItem>>
    func delete(id: String) async -> Outcome<Void>
}
