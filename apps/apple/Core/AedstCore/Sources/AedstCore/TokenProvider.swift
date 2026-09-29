/// The seam `AedstNetwork`'s `AuthInterceptor` calls into to attach/refresh
/// the bearer token. Implemented by `AedstPersistence`'s Keychain-backed
/// store. Lives in `AedstCore` (not `AedstNetwork` or `AedstPersistence`) so
/// neither of those two modules has to depend on the other.
public protocol TokenProvider: Sendable {
    func currentAccessToken() async -> String?
    /// Called once, by whichever request hit a 401 first; concurrent callers
    /// await the same in-flight refresh instead of each firing their own.
    /// Returns `nil` when the refresh token itself is invalid/expired.
    func refreshAccessToken() async -> String?
    func clearTokens() async
}

/// `TokenProvider` plus the write side (`AuthInterceptor`/`AuthService.Login`
/// consumers only ever *read*; `Features/Auth`'s repository is the only
/// caller that *writes* a freshly issued token pair). Kept as a separate,
/// narrower protocol so a read-only consumer can't accidentally overwrite
/// the session.
public protocol SessionStore: TokenProvider {
    func save(accessToken: String, refreshToken: String) async
    /// Only `AuthService.Logout` needs the raw refresh token (the proto
    /// call takes it explicitly, see `auth.proto`'s `LogoutRequest`) — no
    /// other caller should read it, which is why it isn't on `TokenProvider`.
    func currentRefreshToken() async -> String?
}
