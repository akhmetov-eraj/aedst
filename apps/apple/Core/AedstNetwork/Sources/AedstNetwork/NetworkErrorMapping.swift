import AedstCore
import Connect

/// The one place a `ConnectError` is allowed to turn into an `AedstError` —
/// feature repositories call this instead of each pattern-matching Connect's
/// `Code` enum themselves.
public enum NetworkErrorMapping {
    public static func map(_ error: Error) -> AedstError {
        guard let connectError = error as? ConnectError else {
            return .unknown(reason: String(describing: error))
        }

        switch connectError.code {
        case .unauthenticated:
            return .unauthorized
        case .permissionDenied:
            // Authenticated but not allowed (e.g. a non-admin calling AdminService):
            // never "your session ended", which would wrongly suggest signing in again.
            return .forbidden
        case .notFound:
            return .notFound
        case .invalidArgument, .failedPrecondition:
            return .validation(fields: [connectError.message ?? "request"])
        case .aborted, .alreadyExists:
            // ABORTED is the optimistic-concurrency conflict (`expected_version`
            // mismatch in TasksService.UpdateTask): re-read and retry.
            return .conflict
        case .resourceExhausted:
            return .rateLimited(retryAfter: nil)
        case .unavailable, .deadlineExceeded, .canceled:
            return .network(reason: connectError.message ?? connectError.code.name)
        default:
            return .unknown(reason: connectError.message ?? connectError.code.name)
        }
    }
}
