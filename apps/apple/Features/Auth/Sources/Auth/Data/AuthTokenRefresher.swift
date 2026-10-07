import AedstCore

/// Turns the server's answer to `RefreshToken` into the tri-state the session
/// store understands. This is the `refreshHandler` given to `KeychainTokenStore`.
///
/// Classification is the important part:
/// - the server says the refresh token is invalid/expired/revoked (unauthorized,
///   or a request it deems malformed) → `.rejected`: the session ends;
/// - anything transient (offline, timeout, 5xx, rate limit) → `.unavailable`:
///   the session is kept and the next 401 tries again.
///
/// There is intentionally **no automatic retry** here: refresh tokens rotate,
/// so replaying a request whose response was lost would present an
/// already-rotated token and could trip the server's reuse detection.
public struct AuthTokenRefresher: Sendable {
    private let remote: any AuthRemoteDataSource
    private let clock: any AedstClock

    public init(remote: any AuthRemoteDataSource, clock: any AedstClock = SystemClock()) {
        self.remote = remote
        self.clock = clock
    }

    public func refresh(refreshToken: String) async -> RefreshResult {
        switch await remote.refresh(refreshToken: refreshToken) {
        case .success(let dto):
            return .refreshed(dto.tokenPair(issuedAt: clock.now))
        case .failure(.unauthorized), .failure(.validation):
            return .rejected
        case .failure:
            return .unavailable
        }
    }
}
