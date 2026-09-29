import Foundation

/// The single error currency for every module boundary in the app — feature
/// modules never let a raw `URLError`, `DecodingError`, or Connect RPC error
/// escape their `api` (public) surface. Mirrors `AedstError` in
/// `apps/android/core/common` and the `domain` error types in
/// `apps/server/internal/modules/*/domain`.
public enum AedstError: Error, Equatable, Sendable {
    case network(reason: String)
    case unauthorized
    case notFound
    case validation(fields: [String])
    case rateLimited(retryAfter: TimeInterval?)
    case decoding(reason: String)
    case unknown(reason: String)

    public var isRetryable: Bool {
        switch self {
        case .network, .rateLimited: true
        case .unauthorized, .notFound, .validation, .decoding, .unknown: false
        }
    }

    /// A coarse, PII-free label safe to attach to analytics events. Never
    /// send `errorDescription` or an associated `reason`/`fields` payload to
    /// analytics — those may carry response bodies or user input.
    public var analyticsCategory: String {
        switch self {
        case .network: "network"
        case .unauthorized: "unauthorized"
        case .notFound: "not_found"
        case .validation: "validation"
        case .rateLimited: "rate_limited"
        case .decoding: "decoding"
        case .unknown: "unknown"
        }
    }
}

extension AedstError: LocalizedError {
    public var errorDescription: String? {
        switch self {
        case .network(let reason): "Network error: \(reason)"
        case .unauthorized: "You need to sign in again."
        case .notFound: "Not found."
        case .validation(let fields): "Invalid: \(fields.joined(separator: ", "))"
        case .rateLimited: "Too many requests — try again shortly."
        case .decoding(let reason): "Malformed response: \(reason)"
        case .unknown(let reason): "Something went wrong: \(reason)"
        }
    }
}
