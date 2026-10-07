import Foundation

/// The single error currency for every module boundary in the app — feature
/// modules never let a raw `URLError`, `DecodingError`, or Connect RPC error
/// escape their public surface. Mirrors `AedstError` in
/// `apps/android/core/common` and the `domain` error types in
/// `apps/server/internal/modules/*/domain`.
///
/// `reason` payloads are for logs only: `errorDescription` (what users see) is
/// always a localized, human sentence and never interpolates them.
public enum AedstError: Error, Equatable, Sendable {
    case network(reason: String)
    case unauthorized
    /// Authenticated, but not allowed (e.g. a non-admin calling an admin RPC).
    case forbidden
    case notFound
    /// The resource changed since it was read (optimistic concurrency) or already exists.
    case conflict
    case validation(fields: [String])
    case rateLimited(retryAfter: TimeInterval?)
    case decoding(reason: String)
    case storage(reason: String)
    case unknown(reason: String)

    public var isRetryable: Bool {
        switch self {
        case .network, .rateLimited: true
        case .unauthorized, .forbidden, .notFound, .conflict, .validation, .decoding, .storage, .unknown: false
        }
    }

    /// A coarse, PII-free label safe to attach to analytics events. Never
    /// send `technicalDescription` or an associated payload to analytics —
    /// those may carry response bodies or user input.
    public var analyticsCategory: String {
        switch self {
        case .network: "network"
        case .unauthorized: "unauthorized"
        case .forbidden: "forbidden"
        case .notFound: "not_found"
        case .conflict: "conflict"
        case .validation: "validation"
        case .rateLimited: "rate_limited"
        case .decoding: "decoding"
        case .storage: "storage"
        case .unknown: "unknown"
        }
    }

    /// Developer-facing detail for logs and diagnostics. Not for UI.
    public var technicalDescription: String {
        switch self {
        case .network(let reason): "network: \(reason)"
        case .unauthorized: "unauthorized"
        case .forbidden: "forbidden"
        case .notFound: "notFound"
        case .conflict: "conflict"
        case .validation(let fields): "validation: \(fields.joined(separator: ", "))"
        case .rateLimited(let retryAfter): "rateLimited(retryAfter: \(retryAfter.map { String($0) } ?? "nil"))"
        case .decoding(let reason): "decoding: \(reason)"
        case .storage(let reason): "storage: \(reason)"
        case .unknown(let reason): "unknown: \(reason)"
        }
    }
}

extension AedstError: LocalizedError {
    public var errorDescription: String? {
        switch self {
        case .network: String(localized: "errorNetwork", bundle: .module)
        case .unauthorized: String(localized: "errorUnauthorized", bundle: .module)
        case .forbidden: String(localized: "errorForbidden", bundle: .module)
        case .notFound: String(localized: "errorNotFound", bundle: .module)
        case .conflict: String(localized: "errorConflict", bundle: .module)
        case .validation: String(localized: "errorValidation", bundle: .module)
        case .rateLimited: String(localized: "errorRateLimited", bundle: .module)
        case .decoding, .unknown: String(localized: "errorUnknown", bundle: .module)
        case .storage: String(localized: "errorStorage", bundle: .module)
        }
    }
}
