import Connect
import AedstCore

/// The one place a `ConnectError` is allowed to turn into an `AedstError` —
/// feature repositories call this instead of each pattern-matching Connect's
/// `Code` enum themselves.
public enum NetworkErrorMapping {
    public static func map(_ error: Error) -> AedstError {
        guard let connectError = error as? ConnectError else {
            return .unknown(reason: String(describing: error))
        }

        switch connectError.code {
        case .unauthenticated, .permissionDenied:
            return .unauthorized
        case .notFound:
            return .notFound
        case .invalidArgument, .failedPrecondition:
            return .validation(fields: [connectError.message ?? "request"])
        case .resourceExhausted:
            return .rateLimited(retryAfter: nil)
        case .unavailable, .deadlineExceeded, .aborted, .canceled:
            return .network(reason: connectError.message ?? connectError.code.description)
        default:
            return .unknown(reason: connectError.message ?? connectError.code.description)
        }
    }
}
