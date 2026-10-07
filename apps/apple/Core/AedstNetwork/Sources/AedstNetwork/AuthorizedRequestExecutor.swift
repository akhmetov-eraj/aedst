import AedstCore
import Connect

/// Runs an authenticated call and, if the server answers "unauthenticated",
/// refreshes the token pair once and **re-issues the original call** — so a
/// merely-expired access token is invisible to the user.
///
/// Rules:
/// - Exactly one retry per call (no loops on a permanently-401 endpoint).
/// - Only `RefreshResult.refreshed` triggers the retry. `.rejected` means the
///   refresh token is dead (the `SessionStore` has already ended the session
///   and emitted `SessionEvent.expired`); `.unavailable` means offline/5xx —
///   both hand the *original* failure back to the caller.
/// - Concurrent 401s share one refresh (`SessionStore` de-duplicates).
///
/// Every feature's remote data source that uses the *authenticated* client
/// routes its unary calls through `executeUnary`.
public struct AuthorizedRequestExecutor: Sendable {
    private let tokenProvider: any TokenProvider

    public init(tokenProvider: any TokenProvider) {
        self.tokenProvider = tokenProvider
    }

    /// Transport-agnostic core (unit-tested with plain values).
    public func execute<Response>(
        _ call: () async -> Response,
        isUnauthenticated: (Response) -> Bool
    ) async -> Response {
        let first = await call()
        guard isUnauthenticated(first) else { return first }

        switch await tokenProvider.refreshAccessToken() {
        case .refreshed:
            return await call()
        case .rejected, .unavailable:
            return first
        }
    }

    /// Convenience for generated Connect unary calls.
    public func executeUnary<Output: ProtobufMessage>(
        _ call: () async -> ResponseMessage<Output>
    ) async -> ResponseMessage<Output> {
        await execute(call, isUnauthenticated: { $0.code == .unauthenticated })
    }
}
